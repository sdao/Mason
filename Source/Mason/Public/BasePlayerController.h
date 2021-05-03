// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LdrawPartInfo.h"

#include <mutex>

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Delegates/Delegate.h"
#include "BasePlayerController.generated.h"

class UStaticMesh;

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

public:
	UFUNCTION(BlueprintCallable, Category = "Ldraw")
	UStaticMesh* GetLdrawMesh(const FString& name);

	UFUNCTION(BlueprintCallable, Category = "Ldraw")
	TArray<FLdrawPartInfo> GetLdrawPartList();

private:
	TMap<FString, UStaticMesh*> _cache;
};
