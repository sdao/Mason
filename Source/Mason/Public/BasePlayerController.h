// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LdrawPartInfo.h"

#include <mutex>

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Delegates/Delegate.h"

#include "InputRouter.h"

#include "BasePlayerController.generated.h"


class UStaticMesh;
class UInteractiveToolsContext;
class FRuntimeToolsContextQueriesImpl;
class FRuntimeToolsContextTransactionImpl;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFinishLoadDelegate);

/**
 * 
 */
UCLASS()
class MASON_API ABasePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Materials)
	UMaterial* SingleSidedMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Materials)
	UMaterial* DoubleSidedMaterial = nullptr;

	UPROPERTY(BlueprintAssignable, Category = "Ldraw")
	FFinishLoadDelegate OnFinishLoadLdrawPartNames;

	UPROPERTY()
	UInteractiveToolsContext* ToolsContext;

	void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Ldraw")
	UStaticMesh* GetLdrawMesh(const FString& name);

	UFUNCTION(BlueprintCallable, Category = "Ldraw")
	TArray<FLdrawPartInfo> GetLdrawPartList();

	UFUNCTION(BlueprintCallable, Category = "Tool Framework")
	void ShowTransformGizmo(AActor* controlled);

	UFUNCTION(BlueprintCallable, Category = "Tool Framework")
	void HideAllGizmos();
	
	void Tick(float DeltaTime) override;

private:
	TMap<FString, UStaticMesh*> _cache;
	TSharedPtr<FRuntimeToolsContextQueriesImpl> _contextQueriesAPI;
	TSharedPtr<FRuntimeToolsContextTransactionImpl> _contextTransactionsAPI;

	FInputDeviceState CurrentMouseState;
	FVector2D PrevMousePosition = FVector2D::ZeroVector;
};
