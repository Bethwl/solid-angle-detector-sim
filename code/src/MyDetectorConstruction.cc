// =========================================================================
// Header Definition
// =========================================================================

// 1. Include Local headers
#include "MyDetectorConstruction.hh" // Geometry definition class
#include "MySensitiveDetector.hh"    // Custom Sensitive Detector

// 2. Include Geant4 headers
#include "G4NistManager.hh"          // Material database
#include "G4Box.hh"                  // Box geometry
#include "G4Tubs.hh"                 // Cylinder geometry
#include "G4LogicalVolume.hh"        // Logical volume
#include "G4VPhysicalVolume.hh"
#include "G4PVPlacement.hh"          // Physical volume placement
#include "G4SubtractionSolid.hh"     // Boolean subtraction solid
#include "G4VisAttributes.hh"        // Visualization attributes
#include "G4Colour.hh"               // Colors
#include "G4Region.hh"               // Regions
#include "G4SDManager.hh"            // Sensitive detector manager
#include "G4SystemOfUnits.hh"        // Units
#include "G4RegionStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4IStore.hh"
#include "G4RunManager.hh"           // Run manager
#include "G4OpticalSurface.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4ProductionCuts.hh"

// =========================================================================
// Geometry Dimensions
// =========================================================================
namespace GEO {
    // --- 1. WORLD DIMENSIONS ---
    const G4double WorldSize = 3 * m;

    // --- 2. SYSTEM DIMENSIONS ---
    // a) NaI Detector
    // i) Solid NaI Cylinder
    const G4double Solid_NaI_Radius = 39 * mm;
    const G4double Solid_NaI_Height = 78 * mm;

	// ii) Aluminium Oxide Layer (Solid)
	const G4double Al2O3_Thick = 1 * mm;
	const G4double Solid_Al2O3_Radius = Solid_NaI_Radius + Al2O3_Thick;
	const G4double Solid_Al2O3_Height = Solid_NaI_Height + 2 * Al2O3_Thick;
    
    // iii) Aluminium Oxide Layer (Hole)
	const G4double Hole_Al2O3_Radius = Solid_NaI_Radius;
	const G4double Hole_Al2O3_Height = Solid_NaI_Height;

	// iv) Silicon Wafer
	const G4double Si_Thick = 1 * mm;
	const G4double Solid_Si_Radius = Solid_Al2O3_Radius;
	const G4double Solid_Si_Height = Si_Thick;
    
    // iV) Hole in Aluminium Cylinder
    const G4double Hole_Alu_Radius = Solid_Al2O3_Radius;
    const G4double Hole_Alu_Height = Solid_Al2O3_Height + Si_Thick;
    const G4double Alu_Thick = 1 * mm;

    // v) Solid Aluminium Cylinder
    const G4double Solid_Alu_Radius = Hole_Alu_Radius + Alu_Thick;
    const G4double Solid_Alu_Height = Hole_Alu_Height + 2 * Alu_Thick;  

    // b) Source Radiation
    const G4double Souce_Radius = 60 * mm;
    const G4double Source_Height = 30 * mm;

    // c) Lead Chamber (2nd Layer)
    // i) Solid Lead Cylinder
    const G4double Solid_Lead_Radius = 195.5 * mm;
    const G4double Solid_Lead_Height = 460 * mm; // begin with 322 mm then extend to 460 mm to do Task 2
    const G4double Lead_Thick = 30 * mm;

    // ii) Hole in Lead Cylinder
    const G4double Hole_Lead_Radius = Solid_Lead_Radius - Lead_Thick;
    const G4double Hole_Lead_Height = Solid_Lead_Height - 2 * Lead_Thick;

    // d) Copper Chamber (1st Layer)
    // i) Solid Copper Cylinder
    const G4double Solid_Copper_Radius = Hole_Lead_Radius;
    const G4double Solid_Copper_Height = Hole_Lead_Height;
    const G4double Copper_Thick = 1 * mm;

    // ii) Hole in Copper Cylinder
    const G4double Hole_Copper_Radius = Solid_Copper_Radius - Copper_Thick;
    const G4double Hole_Copper_Height = Solid_Copper_Height - 2 * Copper_Thick;

	// --- 3. Sample Dimensions ---
	const G4double Sample_Radius = 75 * mm;
	const G4double Sample_Height = 10 * mm;
}

// =========================================================================
// Constructor / Destructor
// =========================================================================
MyDetectorConstruction::MyDetectorConstruction() 
    : G4VUserDetectorConstruction(),
    fMessenger(nullptr),
    fSample_Detector_Distance(200.0 * mm), // Default HDPE+hBN Layer Thickness
    fSample_Thickness(0.5 * cm),           // Default Sample Thickness = 0.5 cm (5.0 mm)
    fSample_Material("S0"),
    Sample_LV(nullptr)
{
    // Messenger for controlling geometry parameters (Material, Thickness, Distance)
    fMessenger = new G4GenericMessenger(this, "/DetectorConstruction/", "Detector Construction Control");
    fMessenger->DeclarePropertyWithUnit("Sample_Detector_Distance", "cm", fSample_Detector_Distance, "Set Sample-Detector Distance");
    fMessenger->DeclarePropertyWithUnit("Distance", "cm", fSample_Detector_Distance, "Set Sample-Detector Distance (alias)");
    fMessenger->DeclarePropertyWithUnit("distance", "cm", fSample_Detector_Distance, "Set Sample-Detector Distance (alias)");
    fMessenger->DeclarePropertyWithUnit("Sample_Thickness", "cm", fSample_Thickness, "Set Sample Thickness");
    fMessenger->DeclarePropertyWithUnit("Thickness", "cm", fSample_Thickness, "Set Sample Thickness (alias)");
    fMessenger->DeclarePropertyWithUnit("thickness", "cm", fSample_Thickness, "Set Sample Thickness (alias)");
    fMessenger->DeclareProperty("Sample_Material", fSample_Material, "Set Sample Material");
    fMessenger->DeclareProperty("material", fSample_Material, "Set Sample Material (alias)");
    fMessenger->DeclareProperty("Material", fSample_Material, "Set Sample Material (alias)");
    fMessenger->DeclareMethod("update", &MyDetectorConstruction::UpdateGeometry, "Update geometry after parameter changes");

}
MyDetectorConstruction::~MyDetectorConstruction() {}

// =========================================================================
// Define Materials
// =========================================================================
void MyDetectorConstruction::DefineMaterials() {      
    G4NistManager* NIST = G4NistManager::Instance();
    NIST->FindOrBuildMaterial("G4_AIR");
    NIST->FindOrBuildMaterial("G4_Galactic");
    NIST->FindOrBuildMaterial("G4_SODIUM_IODIDE");
	NIST->FindOrBuildMaterial("G4_Al");
    NIST->FindOrBuildMaterial("G4_Cu");
	NIST->FindOrBuildMaterial("G4_Pb");
	NIST->FindOrBuildMaterial("G4_ALUMINUM_OXIDE");
	NIST->FindOrBuildMaterial("G4_Si");
	// Define Radium-226 Material
    G4Isotope* Ra226 = new G4Isotope("Ra226", 88, 226, 226.0 * g / mole);
    G4Element* elRa = new G4Element("Radium", "Ra", 1);
    elRa->AddIsotope(Ra226, 100. * perCent);
    G4Material* matRa226 = new G4Material("Radium226", 5.5 * g / cm3, 1);
    matRa226->AddElement(elRa, 1);

    // --- S0: nosample (G4_AIR) ---
    if (!G4Material::GetMaterial("S0", false)) {
        G4Material* air = NIST->FindOrBuildMaterial("G4_AIR");
        G4Material* S0 = new G4Material("S0", air->GetDensity(), 1, kStateGas);
        S0->AddMaterial(air, 100.0 * perCent);
    }

    // --- Sample Material ---

    // Nguyên tố cho FeO2
    G4Element* elFe = NIST->FindOrBuildElement("Fe");
    G4Element* elO = NIST->FindOrBuildElement("O");

    // a. Cát (Sand) -> Dùng SiO2 (Silicon Dioxide)
    G4Material* matSand = NIST->FindOrBuildMaterial("G4_SILICON_DIOXIDE");

    // b. Xi măng (Cement) -> Dùng tạm G4_CONCRETE (hoặc bạn có thể định nghĩa Portland riêng)
    G4Material* matCement = NIST->FindOrBuildMaterial("G4_CONCRETE");

    // c. FeO2 (Giả định mật độ hạt bột là 5.0 g/cm3 để tính toán, không ảnh hưởng mật độ hỗn hợp sau này)
    G4Material* matFeO2 = G4Material::GetMaterial("FeO2_Powder", false);
    if (!matFeO2) {
        matFeO2 = new G4Material("FeO2_Powder", 5.0 * g / cm3, 2);
        matFeO2->AddElement(elFe, 1);
        matFeO2->AddElement(elO, 2);
    }

    // --- ĐỊNH NGHĨA CÁC VẬT LIỆU HỖN HỢP (CF10 -> CF50) ---

    // CF10: 45% Sand, 45% Cement, 10% FeO2 - Mật độ 2.19 g/cm3
    if (!G4Material::GetMaterial("CF10", false)) {
        G4Material* CF10 = new G4Material("CF10", 2.19 * g / cm3, 3);
        CF10->AddMaterial(matSand, 45 * perCent);
        CF10->AddMaterial(matCement, 45 * perCent);
        CF10->AddMaterial(matFeO2, 10 * perCent);
    }

    // CF20: 40% Sand, 40% Cement, 20% FeO2 - Mật độ 2.22 g/cm3
    if (!G4Material::GetMaterial("CF20", false)) {
        G4Material* CF20 = new G4Material("CF20", 2.22 * g / cm3, 3);
        CF20->AddMaterial(matSand, 40 * perCent);
        CF20->AddMaterial(matCement, 40 * perCent);
        CF20->AddMaterial(matFeO2, 20 * perCent);
    }

    // CF30: 35% Sand, 35% Cement, 30% FeO2 - Mật độ 2.26 g/cm3
    if (!G4Material::GetMaterial("CF30", false)) {
        G4Material* CF30 = new G4Material("CF30", 2.26 * g / cm3, 3);
        CF30->AddMaterial(matSand, 35 * perCent);
        CF30->AddMaterial(matCement, 35 * perCent);
        CF30->AddMaterial(matFeO2, 30 * perCent);
    }

    // CF40: 30% Sand, 30% Cement, 40% FeO2 - Mật độ 2.31 g/cm3
    if (!G4Material::GetMaterial("CF40", false)) {
        G4Material* CF40 = new G4Material("CF40", 2.31 * g / cm3, 3);
        CF40->AddMaterial(matSand, 30 * perCent);
        CF40->AddMaterial(matCement, 30 * perCent);
        CF40->AddMaterial(matFeO2, 40 * perCent);
    }

    // CF50: 25% Sand, 25% Cement, 50% FeO2 - Mật độ 2.53 g/cm3
    if (!G4Material::GetMaterial("CF50", false)) {
        G4Material* CF50 = new G4Material("CF50", 2.53 * g / cm3, 3);
        CF50->AddMaterial(matSand, 25 * perCent);
        CF50->AddMaterial(matCement, 25 * perCent);
        CF50->AddMaterial(matFeO2, 50 * perCent);
    }

	// --- ĐỊNH NGHĨA 8 VẬT LIỆU MAX PHASE (S1 -> S8) ---

	// [1]. S1: Ti2AlC
	if (!G4Material::GetMaterial("S1", false)) {
		G4Material* S1 = new G4Material("S1", 4.1 * g / cm3, 3);
		S1->AddElement(NIST->FindOrBuildElement("Ti"), 0.7106);
		S1->AddElement(NIST->FindOrBuildElement("Al"), 0.2003);
		S1->AddElement(NIST->FindOrBuildElement("C"), 0.0892);
	}

	// [2]. S2: Ti2AlN
	if (!G4Material::GetMaterial("S2", false)) {
		G4Material* S2 = new G4Material("S2", 4.3 * g / cm3, 3);
		S2->AddElement(NIST->FindOrBuildElement("Ti"), 0.7002);
		S2->AddElement(NIST->FindOrBuildElement("Al"), 0.1974);
		S2->AddElement(NIST->FindOrBuildElement("N"), 0.1025);
	}

	// [3]. S3: V2AlC
	if (!G4Material::GetMaterial("S3", false)) {
		G4Material* S3 = new G4Material("S3", 4.0 * g / cm3, 3);
		S3->AddElement(NIST->FindOrBuildElement("V"), 0.7232);
		S3->AddElement(NIST->FindOrBuildElement("Al"), 0.1915);
		S3->AddElement(NIST->FindOrBuildElement("C"), 0.0853);
	}

	// [4]. S4: Cr2AlC
	if (!G4Material::GetMaterial("S4", false)) {
		G4Material* S4 = new G4Material("S4", 5.17 * g / cm3, 3);
		S4->AddElement(NIST->FindOrBuildElement("Cr"), 0.7273);
		S4->AddElement(NIST->FindOrBuildElement("Al"), 0.1887);
		S4->AddElement(NIST->FindOrBuildElement("C"), 0.0840);
	}

	// [5]. S5: Nb2AlC
	if (!G4Material::GetMaterial("S5", false)) {
		G4Material* S5 = new G4Material("S5", 6.44 * g / cm3, 3);
		S5->AddElement(NIST->FindOrBuildElement("Nb"), 0.8265);
		S5->AddElement(NIST->FindOrBuildElement("Al"), 0.1200);
		S5->AddElement(NIST->FindOrBuildElement("C"), 0.0534);
	}

	// [6]. S6: Ta2AlC
	if (!G4Material::GetMaterial("S6", false)) {
		G4Material* S6 = new G4Material("S6", 11.46 * g / cm3, 3);
		S6->AddElement(NIST->FindOrBuildElement("Ta"), 0.9027);
		S6->AddElement(NIST->FindOrBuildElement("Al"), 0.0673);
		S6->AddElement(NIST->FindOrBuildElement("C"), 0.0300);
	}

	// [7]. S7: Ti3SiC2
	if (!G4Material::GetMaterial("S7", false)) {
		G4Material* S7 = new G4Material("S7", 4.5 * g / cm3, 3);
		S7->AddElement(NIST->FindOrBuildElement("Ti"), 0.7338);
		S7->AddElement(NIST->FindOrBuildElement("Si"), 0.1435);
		S7->AddElement(NIST->FindOrBuildElement("C"), 0.1228);
	}

	// [8]. S8: Ti3AlC2
	if (!G4Material::GetMaterial("S8", false)) {
		G4Material* S8 = new G4Material("S8", 4.21 * g / cm3, 3);
		S8->AddElement(NIST->FindOrBuildElement("Ti"), 0.7379);
		S8->AddElement(NIST->FindOrBuildElement("Al"), 0.1387);
		S8->AddElement(NIST->FindOrBuildElement("C"), 0.1235);
	}
}

// =========================================================================
// Construct Geometry
// =========================================================================
G4VPhysicalVolume* MyDetectorConstruction::Construct() {
    DefineMaterials();
    G4NistManager* NIST = G4NistManager::Instance();
    // --- 1. World Volume ---
    G4Box* solidWorld = new G4Box("World", GEO::WorldSize / 2, GEO::WorldSize / 2, GEO::WorldSize / 2);
    G4LogicalVolume* World_LV = new G4LogicalVolume(solidWorld, NIST->FindOrBuildMaterial("G4_AIR"), "WorldLV");
    World_Phys = new G4PVPlacement(nullptr, {}, World_LV, "World", nullptr, false, 0, true);
    
    // --- 2. NaI Detector ---
    // a) Solid Aluminium Cylinder with Hole
    G4double Alu_PosZ = GEO::Solid_Alu_Height / 2;
    G4Tubs* Solid_Alu = new G4Tubs("Aluminium_Cylinder", 0, GEO::Solid_Alu_Radius, GEO::Solid_Alu_Height / 2, 0 * deg, 360 * deg);
    G4Tubs* Hole_Alu = new G4Tubs("Aluminium_Hole", 0, GEO::Hole_Alu_Radius, GEO::Hole_Alu_Height / 2, 0 * deg, 360 * deg);
    G4SubtractionSolid* Alu_Cylinder = new G4SubtractionSolid("Aluminium_Cylinder_With_Hole", Solid_Alu, Hole_Alu, nullptr, G4ThreeVector(0, 0, 0));
    G4LogicalVolume* Alu_LV = new G4LogicalVolume(Alu_Cylinder, NIST->FindMaterial("G4_Al"), "Aluminium_LV");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Alu_PosZ), Alu_LV, "Aluminium_Phys", World_LV, false, 0, true);
    
    // b) Solid NaI Cylinder
    G4double NaI_PosZ = Alu_PosZ - 0.5 * mm; 
    G4Tubs* Solid_NaI = new G4Tubs("NaI_Cylinder", 0, GEO::Solid_NaI_Radius, GEO::Solid_NaI_Height / 2, 0 * deg, 360 * deg);
    G4LogicalVolume* NaI_LV = new G4LogicalVolume(Solid_NaI, NIST->FindMaterial("G4_SODIUM_IODIDE"), "NaI_LV");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, NaI_PosZ), NaI_LV, "NaI_Detector", World_LV, false, 0, true);

	// c) Aluminium Oxide Layer (Solid)
	G4Tubs* Solid_Al2O3 = new G4Tubs("Al2O3_Cylinder", 0, GEO::Solid_Al2O3_Radius, GEO::Solid_Al2O3_Height / 2, 0 * deg, 360 * deg);
	G4Tubs* Hole_Al2O3 = new G4Tubs("Al2O3_Hole", 0, GEO::Hole_Al2O3_Radius, GEO::Hole_Al2O3_Height / 2, 0 * deg, 360 * deg);
	G4SubtractionSolid* Al2O3_Cylinder = new G4SubtractionSolid("Al2O3_Cylinder_With_Hole", Solid_Al2O3, Hole_Al2O3, nullptr, G4ThreeVector(0, 0, 0));
	G4LogicalVolume* Al2O3_LV = new G4LogicalVolume(Al2O3_Cylinder, NIST->FindMaterial("G4_ALUMINUM_OXIDE"), "Al2O3_LV");
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, NaI_PosZ), Al2O3_LV, "Al2O3_Phys", World_LV, false, 0, true);

	// d) Silicon Wafer
	G4double Si_PosZ = NaI_PosZ + GEO::Solid_Al2O3_Height / 2 + GEO::Solid_Si_Height / 2;
	G4Tubs* Solid_Si = new G4Tubs("Si_Cylinder", 0, GEO::Solid_Si_Radius, GEO::Solid_Si_Height / 2, 0 * deg, 360 * deg);
	G4LogicalVolume* Si_LV = new G4LogicalVolume(Solid_Si, NIST->FindMaterial("G4_Si"), "Si_LV");
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Si_PosZ), Si_LV, "Si_Phys", World_LV, false, 0, true);

    // --- 3. Source Radiation ---
    G4double Source_PosZ = GEO::Solid_Alu_Height + 300 * mm;
    G4Tubs* Solid_Source = new G4Tubs("Source_Cylinder", 0, GEO::Souce_Radius, GEO::Source_Height / 2, 0 * deg, 360 * deg);
    G4LogicalVolume* Source_LV = new G4LogicalVolume(Solid_Source, NIST->FindMaterial("G4_Galactic"), "Source_LV");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Source_PosZ), Source_LV, "Source", World_LV, false, 0, true);

    // --- 4. Copper Chamber (1st Layer) ---
    // a) Solid Copper Cylinder with Hole
    G4double Copper_PosZ = GEO::Solid_Copper_Height / 2 - GEO::Copper_Thick;
    G4Tubs* Solid_Copper = new G4Tubs("Copper_Cylinder", 0, GEO::Solid_Copper_Radius, GEO::Solid_Copper_Height / 2, 0 * deg, 360 * deg);
    G4Tubs* Hole_Copper = new G4Tubs("Copper_Hole", 0, GEO::Hole_Copper_Radius, GEO::Hole_Copper_Height / 2, 0 * deg, 360 * deg);
    G4SubtractionSolid* Copper_Cylinder = new G4SubtractionSolid("Copper_Cylinder_With_Hole", Solid_Copper, Hole_Copper, nullptr, G4ThreeVector(0, 0, 0));
    G4LogicalVolume* Copper_LV = new G4LogicalVolume(Copper_Cylinder, NIST->FindMaterial("G4_Cu"), "Copper_LV");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Copper_PosZ), Copper_LV, "Copper", World_LV, false, 0, true);

    // --- 5. Lead Chamber (2nd Layer) ---
    // a) Solid Lead Cylinder with Hole
    G4double Lead_PosZ = Copper_PosZ;
    G4Tubs* Solid_Lead = new G4Tubs("Lead_Cylinder", 0, GEO::Solid_Lead_Radius, GEO::Solid_Lead_Height / 2, 0 * deg, 360 * deg);
    G4Tubs* Hole_Lead = new G4Tubs("Lead_Hole", 0, GEO::Hole_Lead_Radius, GEO::Hole_Lead_Height / 2, 0 * deg, 360 * deg);
    G4SubtractionSolid* Lead_Cylinder = new G4SubtractionSolid("Lead_Cylinder_With_Hole", Solid_Lead, Hole_Lead, nullptr, G4ThreeVector(0, 0, 0));
    G4LogicalVolume* Lead_LV = new G4LogicalVolume(Lead_Cylinder, NIST->FindMaterial("G4_Pb"), "Lead_LV");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Lead_PosZ), Lead_LV, "Lead", World_LV, false, 0, true);

    // --- 6. Concrete / MAX Phase Sample ---
	G4double Sample_PosZ = GEO::Solid_Alu_Height + fSample_Detector_Distance;
	G4double actual_thickness = (fSample_Thickness > 0.0) ? fSample_Thickness : 0.001 * mm;
	G4Tubs* Solid_Sample = new G4Tubs("Sample_Cylinder", 0, GEO::Sample_Radius, actual_thickness / 2, 0 * deg, 360 * deg);
    G4Material* matSample = G4Material::GetMaterial(fSample_Material, false);
    if (!matSample) {
        matSample = NIST->FindOrBuildMaterial(fSample_Material);
    }
    if (!matSample) {
        G4cout << "[WARNING] Material '" << fSample_Material << "' not found, defaulting to S0 (G4_AIR)!" << G4endl;
        matSample = G4Material::GetMaterial("S0", false);
    }
	Sample_LV = new G4LogicalVolume(Solid_Sample, matSample, "Sample_LV");
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Sample_PosZ), Sample_LV, "Sample", World_LV, false, 0, true);

    // --- 7. Lead Collimator ---
    G4double Collimator_Inner_Radius = GEO::Sample_Radius;
    G4double Collimator_OutterRadius = Collimator_Inner_Radius + 37.5 * mm;
    G4double Collimator_Height = (300 * mm - fSample_Detector_Distance) + 30 * mm;
    G4double Collimator_PosZ = Source_PosZ - (300 * mm - fSample_Detector_Distance) / 2;
    G4Tubs* Solid_Collimator = new G4Tubs("Collimator_Cylinder", Collimator_Inner_Radius, Collimator_OutterRadius, Collimator_Height / 2, 0 * deg, 360 * deg);
    G4LogicalVolume* Collimator_LV = new G4LogicalVolume(Solid_Collimator, NIST->FindMaterial("G4_Pb"), "Collimator_LV");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Collimator_PosZ), Collimator_LV, "Collimator", World_LV, false, 0, true);

    // --- 7. Visualization Attributes ---
	// a) World Volume is invisible
	World_LV->SetVisAttributes(G4VisAttributes::GetInvisible());

    // b) Aluminium Cylinder
    G4VisAttributes* Alu_VisAtt = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9, 1.0)); // Grey
	Alu_VisAtt->SetForceSolid(true);
    Alu_LV->SetVisAttributes(Alu_VisAtt);

	// c) NaI Detector
    G4VisAttributes* NaI_VisAtt = new G4VisAttributes(G4Colour(0.5, 0.0, 0.5, 1.0)); // Green
	NaI_VisAtt->SetForceSolid(true);
    NaI_LV->SetVisAttributes(NaI_VisAtt);

    // d) Source Cylinder
    G4VisAttributes* Source_VisAtt = new G4VisAttributes(G4Colour(1.0, 0.0, 0.0, 1.0)); // Red
	Source_VisAtt->SetForceSolid(true);
    Source_LV->SetVisAttributes(Source_VisAtt);

    // e) Copper Cylinder
    G4VisAttributes* Copper_VisAtt = new G4VisAttributes(G4Colour(0.3, 0.5, 0.1, 1.0)); // Orange
	Copper_VisAtt->SetForceSolid(true);
    Copper_LV->SetVisAttributes(Copper_VisAtt);

    // f) Lead Cylinder
    G4VisAttributes* Lead_VisAtt = new G4VisAttributes(G4Colour(0.8, 0.8, 0.8, 1.0)); // Blue
	Lead_VisAtt->SetForceSolid(true);
    Lead_LV->SetVisAttributes(Lead_VisAtt);

	// g) Aluminium Oxide Layer
	G4VisAttributes* Al2O3_VisAtt = new G4VisAttributes(G4Colour(0.8, 0.4, 0.0, 1.0)); // Blue
	Al2O3_VisAtt->SetForceSolid(true);
	Al2O3_LV->SetVisAttributes(Al2O3_VisAtt);

	// h) Silicon Wafer
	G4VisAttributes* Si_VisAtt = new G4VisAttributes(G4Colour(0.3, 1.0, 1.0, 1.0)); // Cyan
	Si_VisAtt->SetForceSolid(true);
	Si_LV->SetVisAttributes(Si_VisAtt);

	// i) Sample Volume
	G4VisAttributes* Sample_VisAtt = new G4VisAttributes(G4Colour(0.6, 0.6, 0.6, 1.0)); // Green
	Sample_VisAtt->SetForceSolid(true);
	Sample_LV->SetVisAttributes(Sample_VisAtt);

    // j) Collimator Volume
    G4VisAttributes* Collimator_VisAtt = new G4VisAttributes(G4Colour(0.2, 0.9, 0.9, 1.0)); // Dark Grey
    Collimator_VisAtt->SetForceSolid(true);
    Collimator_LV->SetVisAttributes(Collimator_VisAtt);


	// --- 7. Print Geometry Parameter Summary ---
    G4cout << "\n================ MyDetectorConstruction.cc ================\n";
	G4cout << "\n================ Geometry Summary ================\n";
	G4cout << " Source-Detector Distance: " << fSample_Detector_Distance / mm << " mm\n";
    G4cout << " Source Z Position:        " << Source_PosZ / mm << " mm\n";
    G4cout << " Sample Material:          " << fSample_Material << "\n";
	G4cout << "=================================================\n\n";

    return World_Phys;
}

void MyDetectorConstruction::UpdateGeometry() {
    auto runManager = G4RunManager::GetRunManager();
    
    // Update sample material if Sample_LV exists
    if (Sample_LV) {
        G4Material* matSample = G4Material::GetMaterial(fSample_Material, false);
        if (!matSample) {
            matSample = G4NistManager::Instance()->FindOrBuildMaterial(fSample_Material);
        }
        if (matSample) {
            Sample_LV->SetMaterial(matSample);
        }
    }

    runManager->ReinitializeGeometry();         // rebuild geometry
    ConstructSDandField();                      // <--- reattach SDs manually
    // Print updated parameters
	G4cout << "\n================ Updated Geometry ================\n";
	G4cout << " Sample-Detector Distance (D): " << fSample_Detector_Distance / cm << " cm (" << fSample_Detector_Distance / mm << " mm)\n";
	G4cout << " Sample Thickness (T):         " << fSample_Thickness / cm << " cm (" << fSample_Thickness / mm << " mm)\n";
	G4cout << " Sample Material:              " << fSample_Material << "\n";
	G4cout << "==================================================\n\n";
}

// =========================================================================
// Construct Sensitive Detectors
// =========================================================================
void MyDetectorConstruction::ConstructSDandField() {

    G4cout << "\n================ ConstructSDandField() =================\n";

    // --- 1. Lấy SDManager và LogicalVolumeStore ---
    auto sdManager = G4SDManager::GetSDMpointer();
    auto LVS = G4LogicalVolumeStore::GetInstance();

    // --- 2. NaI Detector ---
    G4String SDname = "NaI_SD";
    auto NaI_Detector = dynamic_cast<MySensitiveDetector*>(sdManager->FindSensitiveDetector(SDname, false));
    if (!NaI_Detector) {
        NaI_Detector = new MySensitiveDetector(SDname, "NaIHitsCollection");
        sdManager->AddNewDetector(NaI_Detector);
        //G4cout << "[SD] Created new NaI_Detector" << G4endl;
    }
    else {
        //G4cout << "[SD] Reusing existing NaI_Detector" << G4endl;
    }
    // Gán SD
    for (auto lv : *LVS) {
        if (lv->GetName() == "NaI_LV") {
            lv->SetSensitiveDetector(NaI_Detector);
            //G4cout << "Attached NaI_Detector to " << lv->GetName() << G4endl;
        }
    }

    G4cout << "========================================================\n\n";
}

