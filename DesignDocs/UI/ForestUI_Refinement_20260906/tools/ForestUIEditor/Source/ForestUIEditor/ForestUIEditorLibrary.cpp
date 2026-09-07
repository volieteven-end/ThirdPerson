#include "ForestUIEditorLibrary.h"
#include "Modules/ModuleManager.h"
#include "UI/InventroyWidget.h"
#include "UI/InventorySlotWidget.h"
#include "Components/InventoryComponent.h"
#include "Components/HealthComponent.h"
#include "Items/ItemDefinition.h"
#include "Components/LevelComponent.h"
#include "Components/UniformGridPanel.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Animation/WidgetAnimation.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AI/EnemyCharacter.h"
#include "UI/LockOnMarkerWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Widgets/Layout/SBox.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
IMPLEMENT_MODULE(FDefaultModuleImpl,ForestUIEditor)
namespace {
UWorld* GForestPreviewWorld=nullptr;
TSharedPtr<SWidget> GForestSlateRoot;
UInventoryComponent* GForestTestInventory=nullptr;
UHealthComponent* GForestTestHealth=nullptr;
UItemDefinition* GForestTestItem=nullptr;
void CallText(UUserWidget* W,FName Name,const TCHAR* Value) { if(UFunction* F=W->FindFunction(Name)){struct{FText Text;}P{FText::FromString(Value)};W->ProcessEvent(F,&P);} }
void Check(TSharedPtr<FJsonObject> R,const TCHAR* Name,bool Ok){ R->SetBoolField(Name,Ok); }
}
UUserWidget* UForestUIEditorLibrary::CreateTestWidget(){
UWorld* World=UWorld::CreateWorld(EWorldType::GamePreview,false,FName(*FString::Printf(TEXT("ForestPreview_%u"),FMath::Rand())));
World->AddToRoot(); GForestPreviewWorld=World;
APlayerController* PC=World->SpawnActor<APlayerController>(); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); World->AddController(PC);
APawn* Pawn=World->SpawnActor<APawn>(); PC->Possess(Pawn);
UClass* C=LoadClass<UInventoryWidget>(nullptr,TEXT("/Game/Third/Widget/WBP_Inventory.WBP_Inventory_C"));
UInventoryWidget* W=CreateWidget<UInventoryWidget>(PC,C); if(!W)return nullptr;W->AddToRoot(); if(FSlateApplication::IsInitialized())GForestSlateRoot=W->TakeWidget();
GForestTestInventory=NewObject<UInventoryComponent>(Pawn);GForestTestInventory->RegisterComponent();GForestTestInventory->MaxSlots=20;
GForestTestHealth=NewObject<UHealthComponent>(Pawn);GForestTestHealth->RegisterComponent();GForestTestHealth->MaxHealth=100;GForestTestHealth->CurrentHealth=50;
GForestTestItem=NewObject<UItemDefinition>(World);GForestTestItem->ItemId=TEXT("ForestUITestPotion");GForestTestItem->DisplayName=FText::FromString(TEXT("治疗药水"));GForestTestItem->ItemType=EItemType::Consumable;GForestTestItem->HealAmount=25;GForestTestItem->MaxStack=99;
GForestTestItem->Icon=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Third/Image/T_HealthPotion.T_HealthPotion"));
GForestTestInventory->AddItem(GForestTestItem,3);
W->SetHealthComponent(GForestTestHealth);W->SetInventory(GForestTestInventory);
CallText(W,TEXT("RefreshStaminaText"),TEXT("SP: 78 / 100"));
CallText(W,TEXT("RefreshKillText"),TEXT("击败 10"));
CallText(W,TEXT("RefreshObjectiveText"),TEXT("清理区域\n击败敌人 10 / 20"));
if(UFunction* F=W->FindFunction(TEXT("RefreshStaminaPercent"))){struct{float P;}V{0.78f};W->ProcessEvent(F,&V);}
if(UFunction* F=W->FindFunction(TEXT("RefreshLevelProgress"))){struct{FText T;float P;}V{FText::FromString(TEXT("Lv.1  XP 45 / 100")),0.45f};W->ProcessEvent(F,&V);}
return W;
}
FString UForestUIEditorLibrary::TestInventory(UUserWidget* In,bool ExpectForest){
auto R=MakeShared<FJsonObject>();auto* W=Cast<UInventoryWidget>(In);bool Ok=W&&GForestTestInventory&&GForestTestHealth;
if(Ok){
auto* G=Cast<UUniformGridPanel>(W->GetWidgetFromName(TEXT("InventoryGrid"))); R->SetStringField(TEXT("owning_player"),GetNameSafe(W->GetOwningPlayer())); for(const TCHAR* K:{TEXT("InventoryGrid"),TEXT("InventorySlotWidgetClass"),TEXT("InventoryComponent")}){if(auto* Prop=FindFProperty<FObjectPropertyBase>(W->GetClass(),K))R->SetStringField(FString(TEXT("native_"))+K,GetPathNameSafe(Prop->GetObjectPropertyValue_InContainer(W)));}R->SetBoolField(TEXT("grid_exists"),G!=nullptr);R->SetNumberField(TEXT("grid_count"),G?G->GetChildrenCount():-1);R->SetStringField(TEXT("widget_class"),W->GetClass()->GetPathName());Check(R,TEXT("grid_20"),G&&G->GetChildrenCount()==20);Ok&=G&&G->GetChildrenCount()==20;
auto* P=Cast<UProgressBar>(W->GetWidgetFromName(TEXT("HPbar")));bool HP=P&&FMath::IsNearlyEqual(P->GetPercent(),0.5f);Check(R,TEXT("hp_percent_matches_50"),HP);if(ExpectForest)Ok&=HP;
auto* Slot=G?Cast<UInventorySlotWidget>(G->GetChildAt(0)):nullptr;if(Slot){Slot->NotifyClicked();auto* N=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("SelectedItemNameText")));bool Selected=N&&N->GetText().ToString().Contains(TEXT("治疗药水"));Check(R,TEXT("selection_callback"),Selected);Ok&=Selected;}
W->UseSelectedItem();bool Used=GForestTestHealth->CurrentHealth==75&&GForestTestInventory->Slots.Num()==1&&GForestTestInventory->Slots[0].Count==2;Check(R,TEXT("use_heals_and_decrements"),Used);Ok&=Used;
if(G&&G->GetChildrenCount()>1){auto* Empty=Cast<UInventorySlotWidget>(G->GetChildAt(1));Empty->NotifyClicked();auto* N=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("SelectedItemNameText")));W->UseSelectedItem();bool EmptyNoUse=GForestTestHealth->CurrentHealth==75&&GForestTestInventory->Slots[0].Count==2;Check(R,TEXT("empty_slot_no_use"),EmptyNoUse);Ok&=EmptyNoUse;}
W->SetInventoryPanelOpen(true);auto* Panel=W->GetWidgetFromName(TEXT("InventoryPanel"));bool Open=Panel&&Panel->GetVisibility()!=ESlateVisibility::Collapsed&&Panel->GetVisibility()!=ESlateVisibility::Hidden;Check(R,TEXT("inventory_open"),Open);Ok&=Open;
W->SetInventoryPanelOpen(false);bool Closed=Panel&&(Panel->GetVisibility()==ESlateVisibility::Collapsed||Panel->GetVisibility()==ESlateVisibility::Hidden);Check(R,TEXT("inventory_close"),Closed);Ok&=Closed;
if(ExpectForest){auto* I=Cast<UImage>(W->GetWidgetFromName(TEXT("ForestSelectedItemImage")));Check(R,TEXT("detail_image_present"),I!=nullptr);Ok&=I!=nullptr;
bool Buttons=true;for(const TCHAR* Key:{TEXT("CloseButton"),TEXT("UseButton"),TEXT("DropBUtton"),TEXT("ChooseButtonA"),TEXT("ChooseButtonB"),TEXT("ChooseButtonC"),TEXT("RestartButton"),TEXT("NewGameButton")}){auto* Button=Cast<UButton>(W->GetWidgetFromName(Key));Buttons&=Button&&Button->OnClicked.IsBound();}Check(R,TEXT("eight_buttons_bound"),Buttons);Ok&=Buttons;
if(FSlateApplication::IsInitialized()){auto* Levels=NewObject<ULevelComponent>(GForestTestInventory->GetOwner());Levels->RegisterComponent();W->SetLevelComponent(Levels);for(int32 Step=0;Step<4;++Step)Levels->AddExperience(Levels->GetExperienceToNextLevel());bool Pending=Levels->HasPendingUpgradeChoice();W->ChooseLevelUpgrade(0);auto* Modal=W->GetWidgetFromName(TEXT("LevelUpPanel"));bool Dismissed=Pending&&!Levels->HasPendingUpgradeChoice()&&Modal&&Modal->GetVisibility()==ESlateVisibility::Collapsed;Check(R,TEXT("upgrade_choice_dismisses_modal"),Dismissed);Ok&=Dismissed;}
GForestTestHealth->OnHealthChanged.Broadcast(10,0);bool Zero=P&&P->GetPercent()==0.f;Check(R,TEXT("zero_max_health_guard"),Zero);Ok&=Zero;
GForestTestHealth->OnHealthChanged.Broadcast(150,100);bool Clamp=P&&P->GetPercent()==1.f;Check(R,TEXT("health_percent_clamped"),Clamp);Ok&=Clamp;}
GForestTestHealth->MaxHealth=100;GForestTestHealth->CurrentHealth=50;GForestTestHealth->OnHealthChanged.Broadcast(50,100);GForestTestInventory->Slots[0].Count=3;GForestTestInventory->OnInventoryChanged.Broadcast();if(G&&G->GetChildrenCount()){Cast<UInventorySlotWidget>(G->GetChildAt(0))->NotifyClicked();}
}
R->SetBoolField(TEXT("pass"),Ok);FString S;auto Writer=TJsonWriterFactory<>::Create(&S);FJsonSerializer::Serialize(R,Writer);return S;
}
bool UForestUIEditorLibrary::RenderWidget(UUserWidget* W,const FString& Filename,int32 Width,int32 Height){
if(!W||!FSlateApplication::IsInitialized())return false;
if(auto* InventoryWidget=Cast<UInventoryWidget>(W)) InventoryWidget->SetInventoryPanelOpen(Filename.Contains(TEXT("_inventory")));
if(Filename.Contains(TEXT("_upgrade"))){if(UFunction* F=W->FindFunction(TEXT("ShowLevelUpChoices"))){struct{FText A,AD,B,BD,C,CD;} V{FText::FromString(TEXT("生命强化")),FText::FromString(TEXT("提高生命上限，更从容地应对战斗。")),FText::FromString(TEXT("精力强化")),FText::FromString(TEXT("提高精力上限，延长连续行动时间。")),FText::FromString(TEXT("攻击强化")),FText::FromString(TEXT("提高攻击能力，快速击败敌人。"))};W->ProcessEvent(F,&V);}}else{if(UFunction* F=W->FindFunction(TEXT("HideLevelUpChoices")))W->ProcessEvent(F,nullptr);}
if(auto* Upgrade=W->GetWidgetFromName(TEXT("LevelUpPanel")))Upgrade->SetVisibility(Filename.Contains(TEXT("_upgrade"))?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();FlushRenderingCommands();
TSharedRef<SWidget> Root=SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.16f,0.18f,0.14f,1))[GForestSlateRoot.IsValid()?GForestSlateRoot.ToSharedRef():W->TakeWidget()];
FWidgetRenderer Renderer(true,true);
UTextureRenderTarget2D* RT=FWidgetRenderer::CreateTargetFor(FVector2D(Width,Height),TF_Bilinear,false);
for(int i=0;i<3;i++){W->ForceLayoutPrepass();Renderer.DrawWidget(RT,Root,float(Width)/1920.f,FVector2D(Width,Height),1.f/60.f);FlushRenderingCommands();}
if(auto* Panel=W->GetWidgetFromName(TEXT("InventoryPanel")))UE_LOG(LogTemp,Display,TEXT("FOREST_RENDER %s visibility=%d size=%s opacity=%f"),*Filename,(int32)Panel->GetVisibility(),*Panel->GetCachedGeometry().GetLocalSize().ToString(),Panel->GetRenderOpacity()); TArray<FColor> Pixels;bool Read=RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, []{FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(true);return Flags;}());if(!Read||Pixels.Num()!=Width*Height)return false;for(auto& P:Pixels)P.A=255;
TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Width,Height,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);return FFileHelper::SaveArrayToFile(PNG,*Filename);
}
bool UForestUIEditorLibrary::SetGraphPin(UBlueprint* BP,const FString& NodeName,const FString& PinName,const FString& Value){
if(!BP)return false;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(N&&N->GetName()==NodeName)if(auto* P=N->FindPin(*PinName)){N->Modify();P->GetSchema()->TrySetDefaultValue(*P,Value);FBlueprintEditorUtils::MarkBlueprintAsModified(BP);return true;}return false;
}


void UForestUIEditorLibrary::CleanupTestWidget(UUserWidget* W){GForestSlateRoot.Reset();if(W){W->RemoveFromParent();W->ReleaseSlateResources(true);W->RemoveFromRoot();}if(GForestPreviewWorld){GForestPreviewWorld->DestroyWorld(false);GForestPreviewWorld->RemoveFromRoot();GForestPreviewWorld=nullptr;}}

FSlateBrush UForestUIEditorLibrary::MakeBrush(UObject* Resource,FLinearColor Tint,int32 Width,int32 Height,bool NineSlice){FSlateBrush B;B.ImageType=ESlateBrushImageType::FullColor;B.ImageSize=FVector2f(Width,Height);B.TintColor=FSlateColor(Tint);B.SetResourceObject(Resource);B.DrawAs=NineSlice?ESlateBrushDrawType::Box:ESlateBrushDrawType::Image;B.Margin=FMargin(NineSlice?0.125f:0.f);return B;}

void UForestUIEditorLibrary::FinalizeWidgetBlueprint(UBlueprint* In)
{
    if (auto* BP=Cast<UWidgetBlueprint>(In))
    {
        TSet<FName> Names;
        TArray<UWidget*> Connected; BP->WidgetTree->GetAllWidgets(Connected); for (UWidget* Widget:Connected) Names.Add(Widget->GetFName());
        TArray<UWidget*> Orphans;
        BP->ForEachSourceWidget([&](UWidget* Widget){if(!Names.Contains(Widget->GetFName()))Orphans.Add(Widget);});
        for(UWidget* Orphan:Orphans) Orphan->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
        for (UWidgetAnimation* Animation:BP->Animations) if(Animation) Names.Add(Animation->GetFName());
        for (const FName& Name:Names) if(!BP->WidgetVariableNameToGuidMap.Contains(Name)) BP->OnVariableAdded(Name);
        TArray<FName> OldNames;BP->WidgetVariableNameToGuidMap.GenerateKeyArray(OldNames);
        for(const FName& Name:OldNames) if(!Names.Contains(Name)) BP->OnVariableRemoved(Name);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
}

bool UForestUIEditorLibrary::ConnectGraphPins(UBlueprint* BP,const FString& FromNode,const FString& FromPin,const FString& ToNode,const FString& ToPin){if(!BP)return false;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* Graph:Graphs){UEdGraphPin* From=nullptr;UEdGraphPin* To=nullptr;for(UEdGraphNode* N:Graph->Nodes){if(N&&N->GetName()==FromNode)From=N->FindPin(*FromPin);if(N&&N->GetName()==ToNode)To=N->FindPin(*ToPin);}if(From&&To){Graph->Modify();To->BreakAllPinLinks();bool Result=Graph->GetSchema()->TryCreateConnection(From,To);FBlueprintEditorUtils::MarkBlueprintAsModified(BP);return Result;}}return false;}

FString UForestUIEditorLibrary::TestLockMarker(UUserWidget* Context,bool ExpectRefined)
{
    auto Result=MakeShared<FJsonObject>(); bool Core=Context&&Context->GetWorld(); bool Refined=true;
    TArray<TSharedPtr<FJsonValue>> Cases;
    const TArray<FString> Classes={TEXT("/Script/ThirdPerson.EnemyCharacter"),TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"),TEXT("/Game/Third/Character/BP_EnemyRangedCharacter.BP_EnemyRangedCharacter_C")};
    if(Core) for(int32 Index=0;Index<Classes.Num();++Index)
    {
        auto Case=MakeShared<FJsonObject>(); UClass* Class=LoadClass<AEnemyCharacter>(nullptr,*Classes[Index]);
        Case->SetStringField(TEXT("class"),Classes[Index]);
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy=Class?Context->GetWorld()->SpawnActor<AEnemyCharacter>(Class,FVector(600,Index*250,100),FRotator::ZeroRotator,Params):nullptr;
        if(!Enemy){Core=false;Case->SetBoolField(TEXT("spawned"),false);Cases.Add(MakeShared<FJsonValueObject>(Case));continue;}
        if(auto* P=FindFProperty<FFloatProperty>(Class,TEXT("DropChance")))P->SetPropertyValue_InContainer(Enemy,0.f);
        auto* P=FindFProperty<FObjectPropertyBase>(Class,TEXT("LockOnIndicatorWidget"));
        auto* Marker=P?Cast<UWidgetComponent>(P->GetObjectPropertyValue_InContainer(Enemy)):nullptr;
        bool Initial=Marker&&!Marker->IsVisible()&&Marker->bHiddenInGame;
        Enemy->SetLockOnIndicatorVisible(true);
        bool Visible=Marker&&Marker->IsVisible()&&!Marker->bHiddenInGame;
        Core&=Initial&&Visible;
        Case->SetBoolField(TEXT("initial_hidden"),Initial);Case->SetBoolField(TEXT("lock_shows"),Visible);
        if(Marker)
        {
            FVector Aim=Enemy->GetLockOnAimPoint(), Location=Marker->GetComponentLocation();
            Case->SetStringField(TEXT("aim_world"),Aim.ToString());Case->SetStringField(TEXT("marker_world"),Location.ToString());
            Case->SetNumberField(TEXT("aim_z"),Aim.Z);Case->SetNumberField(TEXT("marker_z"),Location.Z);
            Case->SetStringField(TEXT("socket"),Marker->GetAttachSocketName().ToString());Case->SetNumberField(TEXT("size"),Marker->GetDrawSize().X);
            Marker->AddWorldOffset(FVector(0,0,40)); bool Independent=Enemy->GetLockOnAimPoint().Equals(Aim,0.01f);Marker->SetWorldLocation(Location);
            bool Body=Marker->GetAttachSocketName()==FName(TEXT("spine_03")) || (Marker->GetAttachParent()==Enemy->GetRootComponent()&&FMath::Abs(Location.Z-Enemy->GetActorLocation().Z)<Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*0.6f);
            bool Small=Marker->GetDrawSize().X<=26.f;
            Enemy->AddActorWorldOffset(FVector(30,20,10));bool Follows=Marker->GetComponentLocation().Equals(Location+FVector(30,20,10),0.05f);
            Core&=Follows;Refined&=Independent&&Body&&Small;
            Case->SetBoolField(TEXT("camera_independent"),Independent);Case->SetBoolField(TEXT("body_anchor"),Body);Case->SetBoolField(TEXT("small_marker"),Small);Case->SetBoolField(TEXT("follows_enemy"),Follows);
            Enemy->SetLockOnIndicatorVisible(false);bool Released=!Marker->IsVisible()&&Marker->bHiddenInGame;Core&=Released;Case->SetBoolField(TEXT("unlock_hides"),Released);
            Enemy->SetLockOnIndicatorVisible(true);
            if(UFunction* Death=Enemy->FindFunction(TEXT("HandleDeath")))Enemy->ProcessEvent(Death,nullptr);
            bool HiddenOnDeath=!Marker->IsVisible()&&Marker->bHiddenInGame;Core&=HiddenOnDeath;Case->SetBoolField(TEXT("death_hides"),HiddenOnDeath);
            Enemy->SetLockOnIndicatorVisible(true);bool DeadStaysHidden=!Marker->IsVisible()&&Marker->bHiddenInGame;Refined&=DeadStaysHidden;Case->SetBoolField(TEXT("dead_stays_hidden"),DeadStaysHidden);
        }
        Enemy->Destroy();Cases.Add(MakeShared<FJsonValueObject>(Case));
    }
    Result->SetArrayField(TEXT("cases"),Cases);Result->SetBoolField(TEXT("core"),Core);Result->SetBoolField(TEXT("refined"),Refined);Result->SetBoolField(TEXT("pass"),Core&&(!ExpectRefined||Refined));
    FString Json;auto Writer=TJsonWriterFactory<>::Create(&Json);FJsonSerializer::Serialize(Result,Writer);return Json;
}

bool UForestUIEditorLibrary::RenderLockMarker(UUserWidget* Context,const FString& Filename,bool LightBackground)
{
    if(!Context||!FSlateApplication::IsInitialized())return false;
    auto* Marker=CreateWidget<ULockOnMarkerWidget>(Context->GetOwningPlayer(),ULockOnMarkerWidget::StaticClass());if(!Marker)return false;
    const auto* Defaults=GetDefault<AEnemyCharacter>();auto* SizeProperty=FindFProperty<FFloatProperty>(AEnemyCharacter::StaticClass(),TEXT("LockOnIndicatorSize"));
    const float Size=SizeProperty?SizeProperty->GetPropertyValue_InContainer(Defaults):24.f;
    TSharedRef<SWidget> Root=SNew(SBorder).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(LightBackground?FLinearColor(.65f,.65f,.65f,1):FLinearColor(.028f,.04f,.035f,1))
        [SNew(SBox).WidthOverride(Size).HeightOverride(Size)[Marker->TakeWidget()]];
    constexpr int32 Width=128,Height=128;FWidgetRenderer Renderer(true,true);auto* RT=FWidgetRenderer::CreateTargetFor(FVector2D(Width,Height),TF_Bilinear,false);
    for(int i=0;i<3;++i){Renderer.DrawWidget(RT,Root,1.f,FVector2D(Width,Height),1.f/60.f);FlushRenderingCommands();}
    TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(true);bool Read=RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);if(!Read)return false;
    for(auto& Pixel:Pixels)Pixel.A=255;TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Width,Height,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);return FFileHelper::SaveArrayToFile(PNG,*Filename);
}
