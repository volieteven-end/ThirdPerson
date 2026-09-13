#include "CombatUIAssetTools.h"
#if WITH_EDITOR
#include "MainMenuWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"

namespace CombatUIBuild
{
const FString MenuPath=TEXT("/Game/Third/UI/MainMenu/WBP_MainMenu");
const FString MapPath=TEXT("/Game/Third/UI/MainMenu/L_MainMenu");
template<class T> T* Load(const FString& P) { return LoadObject<T>(nullptr,*(P+TEXT(".")+FPackageName::GetLongPackageAssetName(P))); }
bool Save(UObject* O)
{
    O->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(O->GetOutermost(),O,*FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
template<class T> T* Widget(UWidgetTree* Tree,const TCHAR* Name)
{
    auto* W=Tree->ConstructWidget<T>(T::StaticClass(),FName(Name));
    if (auto* BP=Tree->GetTypedOuter<UWidgetBlueprint>()) BP->WidgetVariableNameToGuidMap.FindOrAdd(W->GetFName(),FGuid::NewGuid());
    return W;
}
bool CompileAndSave(UWidgetBlueprint* BP)
{
    BP->WidgetTree->ForEachWidget([BP](UWidget* W) { BP->WidgetVariableNameToGuidMap.FindOrAdd(W->GetFName(),FGuid::NewGuid()); });
    BP->WidgetTree->ForEachWidget([](UWidget* W) { if (auto* Text=Cast<UTextBlock>(W)) if (Cast<UButton>(W->GetParent())) Text->SetAutoWrapText(false); });
    if (auto* Panel=Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("InventoryPanel"))))
    { Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.018f,.035f,.03f,.985f),6.f,FLinearColor(.38f,.34f,.18f),1.f)); Panel->SetBrushColor(FLinearColor::White); Panel->SetRenderOpacity(1); }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
    return BP->Status!=BS_Error && Save(BP);
}
UTextBlock* Text(UWidgetTree* Tree,const TCHAR* Name,const FString& Value,int32 Size=24)
{
    auto* T=Widget<UTextBlock>(Tree,Name); T->SetText(FText::FromString(Value)); auto Font=T->GetFont(); Font.Size=Size; T->SetFont(Font);
    T->SetColorAndOpacity(FLinearColor(.92f,.88f,.74f,1)); T->SetAutoWrapText(true); return T;
}
UCanvasPanelSlot* Place(UCanvasPanel* Panel,UWidget* W,FVector2D Position,FVector2D Size,FVector2D Anchor=FVector2D::ZeroVector,FVector2D Alignment=FVector2D::ZeroVector)
{
    auto* S=Panel->AddChildToCanvas(W); S->SetAnchors(FAnchors(Anchor.X,Anchor.Y)); S->SetAlignment(Alignment); S->SetPosition(Position); S->SetSize(Size); return S;
}
UButton* Button(UWidgetTree* Tree,const TCHAR* Name,const FString& Label)
{
    auto* B=Widget<UButton>(Tree,Name);
    if (auto* Focus=FindFProperty<FBoolProperty>(UButton::StaticClass(),TEXT("IsFocusable"))) Focus->SetPropertyValue_InContainer(B,false);
    FButtonStyle Style; Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.08f,.1f,.09f,.95f),4.f,FLinearColor(.7f,.52f,.25f),1.5f);
    Style.Hovered=FSlateRoundedBoxBrush(FLinearColor(.2f,.18f,.1f,1),4.f,FLinearColor(1,.8f,.4f),2.f); Style.Pressed=Style.Hovered;
    Style.Disabled=FSlateRoundedBoxBrush(FLinearColor(.035f,.045f,.04f,.85f),4.f,FLinearColor(.18f,.2f,.19f),1.f); B->SetStyle(Style);
    auto* T=Text(Tree,*(FString(Name)+TEXT("Label")),Label,26); T->SetJustification(ETextJustify::Center); B->AddChild(T); return B;
}
bool BuildMenu()
{
    auto* BP=Load<UWidgetBlueprint>(MenuPath);
    if (BP) return CompileAndSave(BP); // Compile without regenerating hand-edited layouts.
    auto* Package=CreatePackage(*MenuPath);
    BP=Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UMainMenuWidget::StaticClass(),Package,TEXT("WBP_MainMenu"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
    if (!BP) return false; FAssetRegistryModule::AssetCreated(BP);
    auto* T=BP->WidgetTree.Get(); auto* Root=Widget<UCanvasPanel>(T,TEXT("MenuCanvas")); T->RootWidget=Root;
    auto* Shade=Widget<UBorder>(T,TEXT("MenuShade")); Shade->SetBrushColor(FLinearColor(.012f,.018f,.02f,.78f)); Place(Root,Shade,FVector2D(64,-175),FVector2D(490,450),FVector2D(0,.5));
    auto* Buttons=Widget<UVerticalBox>(T,TEXT("MenuButtons")); Place(Root,Buttons,FVector2D(100,-145),FVector2D(415,325),FVector2D(0,.5));
    const TCHAR* Names[]={TEXT("StartButton"),TEXT("ContinueButton"),TEXT("QuitButton")}; const TCHAR* Labels[]={TEXT("开始游戏"),TEXT("继续游戏"),TEXT("退出游戏")};
    for (int32 I=0;I<3;++I) { auto* Box=Widget<USizeBox>(T,*FString::Printf(TEXT("ButtonSize%d"),I)); Box->SetHeightOverride(80); Box->AddChild(Button(T,Names[I],Labels[I])); Buttons->AddChildToVerticalBox(Box)->SetPadding(FMargin(0,0,0,20)); }
    Place(Root,Text(T,TEXT("InputHint"),TEXT("↑ ↓  选择      Enter  确认"),18),FVector2D(100,-64),FVector2D(460,35),FVector2D(0,1));
    Place(Root,Text(T,TEXT("ErrorText"),TEXT(""),18),FVector2D(100,195),FVector2D(410,80),FVector2D(0,.5));
    auto* Confirm=Widget<UBorder>(T,TEXT("ConfirmPanel")); Confirm->SetBrushColor(FLinearColor(.025f,.035f,.035f,.99f)); Confirm->SetPadding(FMargin(32));
    Place(Root,Confirm,FVector2D::ZeroVector,FVector2D(660,310),FVector2D(.5,.5),FVector2D(.5,.5))->SetZOrder(5);
    auto* ConfirmBody=Widget<UVerticalBox>(T,TEXT("ConfirmBody")); Confirm->AddChild(ConfirmBody);
    ConfirmBody->AddChildToVerticalBox(Text(T,TEXT("ConfirmTitle"),TEXT("重新开始游戏？"),30))->SetPadding(FMargin(0,0,0,18));
    ConfirmBody->AddChildToVerticalBox(Text(T,TEXT("ConfirmExplanation"),TEXT("这会清除现有正式角色的等级、装备、背包与检查点。\n取消将保留全部进度。"),22))->SetPadding(FMargin(0,0,0,30));
    auto* Row=Widget<UHorizontalBox>(T,TEXT("ConfirmButtons")); ConfirmBody->AddChild(Row);
    Row->AddChildToHorizontalBox(Button(T,TEXT("CancelButton"),TEXT("取消 · Esc")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Row->AddChildToHorizontalBox(Button(T,TEXT("ConfirmButton"),TEXT("确认重开 · Enter")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Confirm->SetVisibility(ESlateVisibility::Collapsed);
    return CompileAndSave(BP);
}
bool BuildPortraitMaterial()
{
    const FString RTPath=TEXT("/Game/Third/UI/Inventory/RT_CharacterPortrait");
    auto* RT=Load<UTextureRenderTarget2D>(RTPath);
    if (!RT) { RT=NewObject<UTextureRenderTarget2D>(CreatePackage(*RTPath),TEXT("RT_CharacterPortrait"),RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(RT); }
    RT->SRGB=false; RT->ClearColor=FLinearColor(0,0,0,1); RT->InitCustomFormat(512,512,PF_FloatRGBA,true); if (!Save(RT)) return false;
    const FString Path=TEXT("/Game/Third/UI/Inventory/M_CharacterPortrait");
    if (auto* Existing=Load<UMaterial>(Path))
    { for (const auto& E:Existing->GetExpressions()) if (auto* T=Cast<UMaterialExpressionTextureSampleParameter2D>(E.Get())) if (T->ParameterName==TEXT("PortraitTexture")) { T->Texture=RT; T->SamplerType=SAMPLERTYPE_LinearColor; } Existing->PostEditChange(); return Save(Existing); }
    auto* M=NewObject<UMaterial>(CreatePackage(*Path),TEXT("M_CharacterPortrait"),RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(M);
    M->MaterialDomain=MD_UI; M->BlendMode=BLEND_Translucent;
    auto* Texture=NewObject<UMaterialExpressionTextureSampleParameter2D>(M); Texture->ParameterName=TEXT("PortraitTexture"); Texture->Texture=RT; Texture->SamplerType=SAMPLERTYPE_LinearColor;
    auto* Alpha=NewObject<UMaterialExpressionOneMinus>(M); Alpha->Input.Connect(4,Texture);
    M->GetExpressionCollection().AddExpression(Texture); M->GetExpressionCollection().AddExpression(Alpha);
    M->GetEditorOnlyData()->EmissiveColor.Connect(0,Texture); M->GetEditorOnlyData()->Opacity.Connect(0,Alpha); M->PostEditChange(); return Save(M);
}
bool BuildInventory()
{
    auto* BP=Load<UWidgetBlueprint>(TEXT("/Game/Third/Widget/WBP_Inventory")); if (!BP) return false;
    auto* T=BP->WidgetTree.Get(); if (T->FindWidget(TEXT("InventoryPages"))) return CompileAndSave(BP);
    auto* Panel=Cast<UBorder>(T->FindWidget(TEXT("InventoryPanel"))); if (!Panel || !Panel->GetContent()) return false;
    BP->Modify(); Panel->Modify(); auto* Original=Panel->GetContent(); Panel->RemoveChild(Original);
    auto* Layout=Widget<UCanvasPanel>(T,TEXT("InventoryLayout")); Panel->AddChild(Layout); Panel->SetPadding(FMargin(20));
    Panel->SetBrushColor(FLinearColor(.018f,.035f,.03f,.97f));
    if (auto* Slot=Cast<UCanvasPanelSlot>(Panel->Slot)) { Slot->SetAnchors(FAnchors(.5f,.5f)); Slot->SetAlignment(FVector2D(.5,.5)); Slot->SetPosition(FVector2D::ZeroVector); Slot->SetSize(FVector2D(1200,780)); }
    Place(Layout,Button(T,TEXT("BagPageButton"),TEXT("背包")),FVector2D(300,0),FVector2D(185,52));
    Place(Layout,Button(T,TEXT("AttributesPageButton"),TEXT("属性")),FVector2D(500,0),FVector2D(185,52));
    auto* Image=Widget<UImage>(T,TEXT("CharacterPortrait")); Place(Layout,Image,FVector2D(0,70),FVector2D(275,360));
    Place(Layout,Text(T,TEXT("PortraitCaption"),TEXT("角色展示\n装备与属性实时同步"),18),FVector2D(18,448),FVector2D(240,70));
    auto* Pages=Widget<UWidgetSwitcher>(T,TEXT("InventoryPages")); Place(Layout,Pages,FVector2D(300,70),FVector2D(840,650));
    auto* Bag=Widget<UScrollBox>(T,TEXT("BagPageScroll"));
    auto* BagSize=Widget<USizeBox>(T,TEXT("BagContentSize")); BagSize->SetWidthOverride(820); BagSize->SetHeightOverride(640); BagSize->AddChild(Original); Bag->AddChild(BagSize); Pages->AddChild(Bag);
    if (auto* Frame=T->FindWidget(TEXT("ForestInventoryFrame"))) if (auto* S=Cast<UCanvasPanelSlot>(Frame->Slot))
    { S->SetAnchors(FAnchors(0,0,1,1)); S->SetAlignment(FVector2D::ZeroVector); S->SetOffsets(FMargin(0)); }
    auto Adjust=[T](const TCHAR* Name,FVector2D Pos,FVector2D Size)
    { if (auto* W=T->FindWidget(Name)) if (auto* S=Cast<UCanvasPanelSlot>(W->Slot)) { S->SetAnchors(FAnchors(0,0)); S->SetAlignment(FVector2D::ZeroVector); S->SetPosition(Pos); S->SetSize(Size); } };
    Adjust(TEXT("Inventory"),FVector2D(20,12),FVector2D(250,40)); Adjust(TEXT("ForestCapacity"),FVector2D(565,20),FVector2D(220,30));
    Adjust(TEXT("ForestHeaderDivider"),FVector2D(20,60),FVector2D(780,1)); Adjust(TEXT("InventoryGrid"),FVector2D(20,85),FVector2D(480,420));
    Adjust(TEXT("ForestDetailsDivider"),FVector2D(520,85),FVector2D(1,430)); Adjust(TEXT("SelectedPanel"),FVector2D(545,85),FVector2D(250,430));
    Adjust(TEXT("ForestEmptySelection"),FVector2D(545,170),FVector2D(250,180)); Adjust(TEXT("InventoryHintText"),FVector2D(20,555),FVector2D(780,35));
    Adjust(TEXT("ForestFooterDivider"),FVector2D(20,540),FVector2D(780,1)); Adjust(TEXT("ForestInventoryHelp"),FVector2D(20,595),FVector2D(780,28));
    auto* Stats=Widget<UScrollBox>(T,TEXT("AttributesScroll")); auto* Attr=Text(T,TEXT("AttributesText"),TEXT("角色属性"),23); Stats->AddChild(Attr); Pages->AddChild(Stats); Pages->SetActiveWidgetIndex(0);
    // Retain the original close button and its existing Blueprint delegate, outside either page.
    if (auto* Close=T->FindWidget(TEXT("CloseButton"))) { Close->RemoveFromParent(); Place(Layout,Close,FVector2D(985,0),FVector2D(155,52)); }
    return CompileAndSave(BP);
}
bool BuildMap()
{
    if (FPackageName::DoesPackageExist(MapPath)) return true;
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/Third/Tutorial/Maps/L_ForestTutorial"),false,true)) return false;
    struct FMesh { UStaticMesh* Mesh; FTransform Transform; TArray<UMaterialInterface*> Materials; };
    TArray<FMesh> Source;
    for (TActorIterator<AStaticMeshActor> It(GEditor->GetEditorWorldContext().World());It;++It)
    { auto* C=It->GetStaticMeshComponent(); if (C->GetStaticMesh()) Source.Add({C->GetStaticMesh(),It->GetActorTransform(),C->GetMaterials()}); }
    // Strong references span NewMap's world teardown; this is visual-only copying, never gameplay actors.
    TArray<TStrongObjectPtr<UObject>> KeepAlive; for (const auto& S:Source) { KeepAlive.Emplace(S.Mesh); for (auto* M:S.Materials) if (M) KeepAlive.Emplace(M); }
    GEditor->NewMap(); UWorld* W=GEditor->GetEditorWorldContext().World(); if (!W) return false;
    W->GetWorldSettings()->DefaultGameMode=AMainMenuGameMode::StaticClass();
    for (const auto& S:Source)
    {
        auto* A=W->SpawnActor<AStaticMeshActor>(); A->GetStaticMeshComponent()->SetStaticMesh(S.Mesh); A->SetActorTransform(S.Transform);
        A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); A->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
        for (int32 I=0;I<S.Materials.Num();++I) A->GetStaticMeshComponent()->SetMaterial(I,S.Materials[I]);
    }
    auto* Sun=W->SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-24,-35,0)); Sun->GetLightComponent()->SetIntensity(5); Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.83f,.63f));
    Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->SetAtmosphereSunLight(true);
    auto* Sky=W->SpawnActor<ASkyLight>(); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable); Sky->GetLightComponent()->SetRealTimeCaptureEnabled(true); Sky->GetLightComponent()->SetIntensity(.8f);
    auto* SkyActor=W->SpawnActor<AActor>(); auto* Atmosphere=NewObject<USkyAtmosphereComponent>(SkyActor); SkyActor->SetRootComponent(Atmosphere); SkyActor->AddInstanceComponent(Atmosphere); Atmosphere->RegisterComponent();
    auto* Post=W->SpawnActor<APostProcessVolume>(); Post->bUnbound=true; Post->Settings.bOverride_AutoExposureBias=true; Post->Settings.AutoExposureBias=-.4f; Post->Settings.bOverride_BloomIntensity=true; Post->Settings.BloomIntensity=.2f;
    const FVector Position(-6500,-4400,760),LookAt(-3400,-1800,180);
    auto* Camera=W->SpawnActor<ACameraActor>(Position,(LookAt-Position).Rotation()); Camera->SetActorLabel(TEXT("MainMenuCamera")); Camera->GetCameraComponent()->SetFieldOfView(65);
    return FEditorFileUtils::SaveLevel(W->PersistentLevel,FPackageName::LongPackageNameToFilename(MapPath,FPackageName::GetMapPackageExtension()));
}
bool CompleteMenuScenery()
{
    if (!FEditorFileUtils::LoadMap(*MapPath,false,true)) return false;
    for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World());It;++It) if (It->ActorHasTag(TEXT("MenuInstancedScenery"))) return true;
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/Third/Tutorial/Maps/L_ForestTutorial"),false,true)) return false;
    struct FBatch { UStaticMesh* Mesh; TArray<UMaterialInterface*> Materials; TArray<FTransform> Transforms; };
    TArray<FBatch> Batches; TArray<FVector> Signs; TArray<TStrongObjectPtr<UObject>> Keep;
    for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World());It;++It)
    {
        if (It->GetActorLabel().Contains(TEXT("Signboard")) || It->GetActorLabel().Contains(TEXT("Signpost"))) Signs.Add(It->GetActorLocation());
        TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
        for (auto* C:Components) if (C->GetStaticMesh())
        {
            FBatch B; B.Mesh=C->GetStaticMesh(); B.Materials=C->GetMaterials(); Keep.Emplace(B.Mesh);
            for (auto* M:B.Materials) if (M) Keep.Emplace(M);
            for (int32 I=0;I<C->GetInstanceCount();++I) { FTransform T; C->GetInstanceTransform(I,T,true); B.Transforms.Add(T); }
            Batches.Add(MoveTemp(B));
        }
    }
    if (!FEditorFileUtils::LoadMap(*MapPath,false,true)) return false;
    auto* W=GEditor->GetEditorWorldContext().World();
    TArray<AActor*> MenuSignboards;
    for (TActorIterator<AStaticMeshActor> It(W);It;++It) if (Signs.ContainsByPredicate([&](FVector P){ return P.Equals(It->GetActorLocation(),.1f); })) MenuSignboards.Add(*It);
    for (auto* Sign:MenuSignboards) W->DestroyActor(Sign); // Only copied, textless tutorial boards in this new presentation map.
    auto* Scenery=W->SpawnActor<AActor>(); Scenery->SetActorLabel(TEXT("MainMenu_ForestAndStonePaths")); Scenery->Tags.Add(TEXT("MenuInstancedScenery"));
    auto* Root=NewObject<USceneComponent>(Scenery); Scenery->SetRootComponent(Root); Scenery->AddInstanceComponent(Root); Root->RegisterComponent();
    for (const auto& B:Batches)
    {
        auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(Scenery); Scenery->AddInstanceComponent(C); C->SetupAttachment(Root);
        C->SetStaticMesh(B.Mesh); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCanEverAffectNavigation(false);
        for (int32 I=0;I<B.Materials.Num();++I) C->SetMaterial(I,B.Materials[I]); C->RegisterComponent();
        C->AddInstances(B.Transforms,false,true,false); C->BuildTreeIfOutdated(false,true);
    }
    for (TActorIterator<ACameraActor> It(W);It;++It)
    { const FVector Position(-4500,-3250,240),LookAt(-1350,-1900,235); It->SetActorLocationAndRotation(Position,(LookAt-Position).Rotation()); It->GetCameraComponent()->SetFieldOfView(65); }
    auto* Fog=W->SpawnActor<AExponentialHeightFog>(); Fog->GetComponent()->SetFogDensity(.016f); Fog->GetComponent()->SetFogHeightFalloff(.2f);
    return FEditorFileUtils::SaveLevel(W->PersistentLevel,FPackageName::LongPackageNameToFilename(MapPath,FPackageName::GetMapPackageExtension()));
}
}
bool UCombatUIAssetTools::InspectUI()
{
    auto* BP=CombatUIBuild::Load<UWidgetBlueprint>(TEXT("/Game/Third/Widget/WBP_Inventory")); if (!BP) return false;
    BP->WidgetTree->ForEachWidget([](UWidget* W) { UE_LOG(LogTemp,Display,TEXT("UI_INSPECT %s class=%s parent=%s"),*W->GetName(),*W->GetClass()->GetName(),*GetNameSafe(W->GetParent())); }); return true;
}
bool UCombatUIAssetTools::BuildUIUpgrade()
{
    const bool OK=CombatUIBuild::BuildMenu() && CombatUIBuild::BuildPortraitMaterial() && CombatUIBuild::BuildInventory() && CombatUIBuild::BuildMap() && CombatUIBuild::CompleteMenuScenery();
    UE_LOG(LogTemp,Display,TEXT("COMBAT_UI_BUILD success=%d"),OK); return OK;
}
#else
bool UCombatUIAssetTools::InspectUI() { return false; }
bool UCombatUIAssetTools::BuildUIUpgrade() { return false; }
#endif
