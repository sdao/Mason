// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LdrawPartInfo.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct MASON_API FLdrawPartInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ldraw")
	FString Path;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ldraw")
	FString Description;

	FLdrawPartInfo();
};
