"""
run_solid_angle_ver3_task.py - Swarm Worker for 2026.03.13_Advanced Solid Angle Detector_ver3
Architecture:
- 13 GitHub Accounts x 12 Runners = 156 Runners (1-to-1 Matrix Partitioning)
- 66,220 Jobs (14 Samples x 11 Distances [10-20cm] x 2 Thicknesses [0.1, 0.5cm] x 215 Energies [196 Gamma + 19 Neutron])
- On-the-fly Macro Generation (Zero disk bloat)
- 1-to-1 Flat Common Pool Streaming to Google Drive 02_Flux_Data
- Batched ROOT sync to 03_Simulation_Outputs
- Benchmark sync to 05_Execution_Logs_and_Benchmarks
"""
import os
import sys
import time
import re
import json
import zipfile
import argparse
import subprocess
from pathlib import Path
import pandas as pd

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)

import gdrive_helper

ACCOUNTS_ORDER = [
    "haoidlemystic1",
    "vuicho",
    "haoidle4zk",
    "67hao",
    "idlemystich",
    "CarwynDuc",
    "laolaolaoma09",
    "Bethwl",
    "coredaohaojack",
    "htvnhe",
    "haohue27",
    "fewhourtorelax",
    "zotactothesun"
]

DEFAULT_DRIVE_FOLDER = "1BB3P7cmGmeDeYdZgpW72h_n8N4GIiaoK"


def parse_args():
    parser = argparse.ArgumentParser(description="Geant4 Solid Angle ver3 Swarm Worker")
    parser.add_argument("--runner-id", type=int, required=True, help="Runner ID on this account (1..12)")
    parser.add_argument("--runners-per-acc", type=int, default=12, help="Number of runners per account")
    parser.add_argument("--account-name", type=str, default=None, help="GitHub account username")
    parser.add_argument("--gdrive-folder", type=str, default=DEFAULT_DRIVE_FOLDER, help="Root Google Drive folder ID")
    parser.add_argument("--primaries", type=int, default=1000000, help="Number of particles per job")
    parser.add_argument("--manifest", type=str, default="code/macros/macro/full_job_matrix_manifest.csv", help="Path to manifest CSV")
    parser.add_argument("--sim-bin", type=str, default="./build/Sim", help="Path to compiled Sim executable")
    parser.add_argument("--timeout-hours", type=float, default=5.4, help="Safe timeout limit in hours")
    parser.add_argument("--skip-drive-scan", action="store_true", help="Skip listing existing files from Drive")
    return parser.parse_args()


def get_existing_drive_files(service, folder_id):
    existing = set()
    page_token = None
    while True:
        try:
            q = f"'{folder_id}' in parents and name contains '.csv' and trashed = false"
            res = service.files().list(
                q=q,
                supportsAllDrives=True,
                includeItemsFromAllDrives=True,
                fields="nextPageToken, files(name)",
                pageSize=1000,
                pageToken=page_token
            ).execute()
            for f in res.get("files", []):
                existing.add(f["name"].lower().strip())
            page_token = res.get("nextPageToken")
            if not page_token:
                break
        except Exception as e:
            print(f">>> [WARNING] Error reading Drive files: {e}")
            break
    return existing


def generate_macro_content(sample, thickness_cm, distance_cm, particle, energy_val, energy_unit, job_name, beam_on):
    return (
        "/control/verbose 0\n"
        "/run/verbose 0\n"
        "/event/verbose 0\n"
        "/tracking/verbose 0\n"
        f"/DetectorConstruction/Sample_Material {sample}\n"
        f"/DetectorConstruction/Sample_Thickness {float(thickness_cm):.6f} cm\n"
        f"/DetectorConstruction/Sample_Detector_Distance {float(distance_cm):.2f} cm\n"
        "/DetectorConstruction/update\n"
        f"/gun/particle {particle}\n"
        f"/gun/energy {energy_val} {energy_unit}\n"
        f"/analysis/setFileName hist_{job_name}\n"
        f"/run/beamOn {int(beam_on)}\n"
    )


def main():
    args = parse_args()
    acc_name = args.account_name or os.environ.get("GITHUB_REPOSITORY_OWNER") or "haoidlemystic1"

    if acc_name in ACCOUNTS_ORDER:
        acc_idx = ACCOUNTS_ORDER.index(acc_name)
    else:
        acc_idx = 0

    runners_per_acc = args.runners_per_acc
    total_runners = len(ACCOUNTS_ORDER) * runners_per_acc
    global_runner_id = acc_idx * runners_per_acc + (args.runner_id - 1)

    print("=" * 80)
    print(f"🚀 STARTING SOLID ANGLE VER3 RUNNER #{args.runner_id} (Account: {acc_name})")
    print(f"Global Runner ID: {global_runner_id + 1} / {total_runners}")
    print(f"Google Drive Project Folder ID: {args.gdrive_folder}")
    print("=" * 80)

    # 1. Locate Sim executable
    sim_bin = Path(args.sim_bin)
    if not sim_bin.exists():
        candidates = [
            Path("./build/Sim"),
            Path("./Sim"),
            Path("./code/build/Release/Sim.exe"),
            Path("./build/Release/Sim.exe"),
            Path("./code/build/Sim")
        ]
        for c in candidates:
            if c.exists():
                sim_bin = c
                break

    print(f">>> [BINARY] Geant4 Sim executable: {sim_bin.resolve()}")
    if not sim_bin.exists():
        raise FileNotFoundError(f"Cannot find Geant4 Sim executable at {sim_bin}")

    # 2. Connect to Google Drive
    drive_service = None
    flux_folder_id = None
    outputs_folder_id = None
    logs_folder_id = None
    existing_drive_files = set()

    try:
        drive_service = gdrive_helper.get_drive_service()
        subfolders = gdrive_helper.get_or_create_subfolders(drive_service, parent_id=args.gdrive_folder)
        flux_folder_id = subfolders.get("02_Flux_Data")
        outputs_folder_id = subfolders.get("03_Simulation_Outputs")
        logs_folder_id = subfolders.get("05_Execution_Logs_and_Benchmarks")

        if not flux_folder_id or not outputs_folder_id:
            raise RuntimeError("Required Google Drive subfolders (02_Flux_Data, 03_Simulation_Outputs) not found.")

        print(">>> [DRIVE] Connected successfully to Google Drive 5-tier architecture:")
        print(f"  • 02_Flux_Data:                     {flux_folder_id}")
        print(f"  • 03_Simulation_Outputs:            {outputs_folder_id}")
        print(f"  • 05_Execution_Logs_and_Benchmarks: {logs_folder_id}")

        if not args.skip_drive_scan:
            existing_drive_files = get_existing_drive_files(drive_service, flux_folder_id)
            print(f">>> [AUTO-RESUME] Found {len(existing_drive_files):,} existing CSV files in 02_Flux_Data.")
        else:
            print(">>> [DIRECT RESCUE] Skipping Drive file listing scan to conserve API quota. Running target manifest directly.")
    except Exception as e:
        print(f">>> [FATAL ERROR] Google Drive connection failed: {e}")
        print(">>> Terminating runner immediately to prevent offline computation without saving to Drive!")
        sys.exit(1)

    # 3. Load Manifest and Partition
    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        alt_paths = [
            Path("code/macros/macro/full_job_matrix_manifest.csv"),
            Path("macros/macro/full_job_matrix_manifest.csv"),
            Path("full_job_matrix_manifest.csv")
        ]
        for ap in alt_paths:
            if ap.exists():
                manifest_path = ap
                break

    print(f">>> [MANIFEST] Loading {manifest_path}...")
    df_manifest = pd.read_csv(manifest_path)
    total_manifest_jobs = len(df_manifest)
    print(f">>> [PLAN] Total jobs across full research: {total_manifest_jobs:,}")

    # Disjoint 1-to-1 Partitioning
    my_indices = [i for i in range(total_manifest_jobs) if i % total_runners == global_runner_id]
    df_my_jobs = df_manifest.iloc[my_indices].copy()
    my_total_count = len(df_my_jobs)

    # Filter out already completed jobs
    todo_rows = []
    for _, row in df_my_jobs.iterrows():
        jname = str(row["Job_Name"]).strip()
        csv_name = f"{jname}.csv".lower()
        if csv_name not in existing_drive_files:
            todo_rows.append(row)

    print(f">>> [RUNNER #{args.runner_id}] Assigned: {my_total_count} jobs | Done: {my_total_count - len(todo_rows)} | Todo: {len(todo_rows)} jobs")

    if not todo_rows:
        print(f"🎉 All {my_total_count} jobs for Runner #{args.runner_id} are already completed! Exiting early.")
        return

    # 4. Simulation Execution Loop
    work_dir = Path("./work_sim").resolve()
    work_dir.mkdir(parents=True, exist_ok=True)

    start_total = time.time()
    success_count = 0
    subproject_cache = {}
    benchmarks = []

    for idx, row in enumerate(todo_rows, 1):
        elapsed_overall = time.time() - start_total
        if elapsed_overall > (args.timeout_hours * 3600 - 600):
            print(f">>> [TIMEOUT SAFEGUARD] Approaching {args.timeout_hours}h limit. Exiting cleanly.")
            break

        jname = str(row["Job_Name"]).strip()
        sample = str(row["Sample"]).strip()
        particle = str(row["Particle"]).strip()
        dist_cm = float(row["Distance_cm"])
        thick_cm = float(row["Thickness_cm"])
        e_val = row["Energy_Value"]
        e_unit = str(row["Unit"]).strip()
        e_kev = float(row["Energy_keV"])
        beam_on = int(args.primaries or row.get("BeamOn", 1000000))

        csv_name = f"{jname}.csv"

        # On-the-fly macro creation
        mac_file = work_dir / f"run_{jname}.mac"
        mac_content = generate_macro_content(sample, thick_cm, dist_cm, particle, e_val, e_unit, jname, beam_on)
        mac_file.write_text(mac_content, encoding="utf-8")

        print(f"\n--- [{idx}/{len(todo_rows)}] Runner #{args.runner_id} ({acc_name}) Running: {jname} ---")
        print(f"    Sample: {sample} | D: {dist_cm} cm | T: {thick_cm} cm | {particle} {e_val} {e_unit} ({e_kev} keV)")

        t0 = time.time()
        cmd = [str(sim_bin.resolve()), str(mac_file.name)]
        res = subprocess.run(cmd, cwd=str(work_dir), capture_output=True, text=True)
        job_elapsed = time.time() - t0

        if res.returncode != 0:
            print(f"    ❌ [SIM ERROR] Exit code {res.returncode}: {(res.stderr or res.stdout)[-300:]}")
            continue

        # Regex parse outputs
        edep_match = re.search(r"\[Edep\]\s*NaI\s*Detector\s*=\s*([0-9.eE+-]+)\s*MeV", res.stdout)
        pulse_match = re.search(r"\[Pulse\]\s*NaI\s*Detector\s*=\s*([0-9.eE+-]+)", res.stdout)
        events_match = re.search(r"Number of events:\s*(\d+)", res.stdout)

        edep_mev = float(edep_match.group(1)) if edep_match else 0.0
        pulse_count = float(pulse_match.group(1)) if pulse_match else 0.0
        events_simulated = int(events_match.group(1)) if events_match else beam_on

        print(f"    ✅ [RESULT] Events: {events_simulated:,} | Edep: {edep_mev:.6e} MeV | Pulse: {pulse_count:.0f} | Time: {job_elapsed:.2f}s")

        # Write 1-to-1 Flat CSV
        csv_file = work_dir / csv_name
        df_row = pd.DataFrame([{
            "job_name": jname,
            "sample": sample,
            "particle": particle,
            "distance_cm": dist_cm,
            "thickness_cm": thick_cm,
            "energy_kev": e_kev,
            "primaries": events_simulated,
            "edep_mev": edep_mev,
            "pulse_count": pulse_count,
            "sim_seconds": round(job_elapsed, 2),
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }])
        df_row.to_csv(csv_file, index=False)

        # Upload CSV to Google Drive 02_Flux_Data
        if drive_service and flux_folder_id:
            try:
                gdrive_helper.upload_file_to_folder(drive_service, str(csv_file), flux_folder_id)
                existing_drive_files.add(csv_name.lower())
                print(f"    📤 Streamed {csv_name} -> 02_Flux_Data")
                try:
                    csv_file.unlink()
                except Exception:
                    pass
            except Exception as e:
                print(f"    ⚠️ Upload CSV failed: {e}")

        # Upload raw simulation outputs (ROOT file) to 03_Simulation_Outputs/<sample>/<jname>/ (NO ZIP)
        root_cand = work_dir / f"hist_{jname}.root"
        if root_cand.exists() and drive_service and outputs_folder_id:
            try:
                if sample not in subproject_cache:
                    subproject_cache[sample] = gdrive_helper.get_or_create_subfolder(drive_service, outputs_folder_id, sample)
                sample_fid = subproject_cache[sample]
                if sample_fid:
                    input_fid = gdrive_helper.get_or_create_subfolder(drive_service, sample_fid, jname)
                    if input_fid:
                        gdrive_helper.upload_file_to_folder(drive_service, str(root_cand), input_fid)
                        print(f"    📁 Output synced -> 03_Simulation_Outputs/{sample}/{jname}/{root_cand.name}")
                try:
                    root_cand.unlink()
                except Exception:
                    pass
            except Exception as e:
                print(f"    ⚠️ Upload output failed: {e}")

        success_count += 1
        benchmarks.append(f"{jname}: {job_elapsed:.2f}s | Events: {events_simulated} | Pulse: {pulse_count}")

        # Clean temp macro
        try:
            mac_file.unlink()
        except Exception:
            pass

    # Benchmark upload
    if benchmarks and drive_service and logs_folder_id:
        try:
            bench_file = work_dir / f"benchmark_{acc_name}_r{args.runner_id:02d}.txt"
            total_dur = time.time() - start_total
            lines = [
                f"Runner #{args.runner_id} on Account {acc_name}",
                f"Completed: {success_count} / {len(todo_rows)} jobs",
                f"Total Duration: {total_dur:.1f}s ({total_dur/60:.1f} min)",
                f"Average per job: {total_dur/max(1, success_count):.2f}s\n",
                "Detailed Job Timing:"
            ] + benchmarks
            bench_file.write_text("\n".join(lines), encoding="utf-8")
            gdrive_helper.upload_file_to_folder(drive_service, str(bench_file), logs_folder_id)
            print(f"    📋 Benchmark log uploaded -> 05_Execution_Logs_and_Benchmarks")
        except Exception as e:
            print(f"    ⚠️ Benchmark upload failed: {e}")

    print("\n" + "=" * 80)
    print(f"🎉 RUNNER #{args.runner_id} FINISHED: {success_count}/{len(todo_rows)} jobs completed successfully!")
    print("=" * 80)


if __name__ == "__main__":
    main()
