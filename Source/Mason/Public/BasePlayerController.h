// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UStaticMesh;

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

public:
	UFUNCTION(BlueprintCallable, Category = "Ldraw")
	UStaticMesh* GetLdrawMesh(const FString& name);

private:
	TMap<FString, UStaticMesh*> _cache;
};
