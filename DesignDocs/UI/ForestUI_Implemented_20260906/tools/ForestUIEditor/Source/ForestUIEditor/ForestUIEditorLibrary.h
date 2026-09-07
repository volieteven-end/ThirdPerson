#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ForestUIEditorLibrary.generated.h"
class UUserWidget;
class UBlueprint;
UCLASS()
class FORESTUIEDITOR_API UForestUIEditorLibrary : public UBlueprintFunctionLibrary {
GENERATED_BODY()
public:
UFUNCTION(BlueprintCallable,Category="ForestUI") static bool ConnectGraphPins(UBlueprint* Blueprint,const FString& FromNode,const FString& FromPin,const FString& ToNode,const FString& ToPin);
UFUNCTION(BlueprintCallable,Category="ForestUI") static void FinalizeWidgetBlueprint(UBlueprint* Blueprint);
UFUNCTION(BlueprintCallable,Category="ForestUI") static FSlateBrush MakeBrush(UObject* Resource,FLinearColor Tint,int32 Width,int32 Height,bool NineSlice);
UFUNCTION(BlueprintCallable,Category="ForestUI") static void CleanupTestWidget(UUserWidget* Widget);
UFUNCTION(BlueprintCallable,Category="ForestUI") static UUserWidget* CreateTestWidget();
UFUNCTION(BlueprintCallable,Category="ForestUI") static FString TestInventory(UUserWidget* Widget,bool ExpectForest);
UFUNCTION(BlueprintCallable,Category="ForestUI") static bool RenderWidget(UUserWidget* Widget,const FString& Filename,int32 Width=1920,int32 Height=1080);
UFUNCTION(BlueprintCallable,Category="ForestUI") static bool SetGraphPin(UBlueprint* Blueprint,const FString& NodeName,const FString& PinName,const FString& Value);
};

