// Fill out your copyright notice in the Description page of Project Settings.


#include "BasePlayerController.h"

#include "Engine/StaticMesh.h"

#include "Async/ParallelFor.h"

#include "Materials/MaterialInstanceConstant.h"

#include "MeshDescription.h"
#include "MeshTypes.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

#include "InteractiveToolsContext.h"
#include "ToolContextInterfaces.h"

#include "BaseGizmos/TransformProxy.h"
#include "BaseGizmos/TransformGizmo.h"

#include "Slate/SGameLayerManager.h" 	
#include "Slate/SceneViewport.h"

#include <string>
#include <sstream>
#include <vector>


namespace {

	enum class Wind { CW, CCW, };
	enum class Certify { Certified, Uncertified, Unknown, };

	void _ImportLdrawFile(const FString& ldrawRoot, const FString& name, const FMatrix& accumTransform, const bool accumCull, const bool accumInvert, const int accumColor, size_t maxReferenceDepth, const FPolygonGroupID& singleSidedGroup, const FPolygonGroupID& doubleSidedGroup, FMeshDescription* desc)
	{
		FString path;
		if (path = FPaths::Combine(ldrawRoot, TEXT("parts"), name), FPaths::FileExists(path)) {
		}
		else if (path = FPaths::Combine(ldrawRoot, TEXT("p"), name), FPaths::FileExists(path)) {
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("Can't find file '%s'"), *name);
			UE_LOG(LogTemp, Warning, TEXT("  Tried '%s'"), *FPaths::Combine(ldrawRoot, TEXT("parts"), name));
			UE_LOG(LogTemp, Warning, TEXT("  Tried '%s'"), *FPaths::Combine(ldrawRoot, TEXT("p"), name));
			return;
		}

		FString contents;
		FFileHelper::LoadFileToString(contents, *path);

		FStaticMeshAttributes staticMeshAttrs(*desc);
		TVertexAttributesRef<FVector> vertexPositions = staticMeshAttrs.GetVertexPositions();

		std::vector<float> buf;

		std::string outputString(TCHAR_TO_UTF8(*contents));
		std::stringstream sstream(outputString);

		bool localCull = true;
		Wind localWinding = Wind::CCW;
		Certify localCertified = Certify::Unknown;
		bool invertNext = false;

		int lineNum = 1;
		for (std::string line; std::getline(sstream, line); lineNum++) {
			std::stringstream lineStream(line);

			int lineType;
			if (!(lineStream >> lineType)) {
				// Blank lines are acceptable, but just skipped.
				continue;
			}

			const bool invertCur = invertNext;
			invertNext = false;

			if (lineType == 0) {
				std::string metaType;
				if (!(lineStream >> metaType)) {
					// Type-0 lines without data are OK, but just skipped.
					continue;
				}

				if (metaType == "BFC") {
					std::vector<std::string> options;

					std::string temp;
					while (lineStream >> temp) {
						options.push_back(temp);
					}

					// If certification unknown and NOCERTIFY not present, then implicitly certify.
					if (localCertified == Certify::Unknown) {
						if (std::find(options.begin(), options.end(), "NOCERTIFY") == options.end()) {
							localCertified = Certify::Certified;
						}
					}

					for (const std::string& opt : options) {
						if (opt == "CERTIFY") {
							if (localCertified == Certify::Uncertified) {
								UE_LOG(LogTemp, Warning, TEXT("Encountered CERTIFY in previously-NOCERTIFY'ed file '%s':%d"), *name, lineNum);
							}
							localCertified = Certify::Certified;
						}
						else if (opt == "NOCERTIFY") {
							if (localCertified == Certify::Certified) {
								UE_LOG(LogTemp, Warning, TEXT("Encountered NOCERTIFY in previously-CERTIFY'ed file '%s':%d"), *name, lineNum);
							}
							localCertified = Certify::Uncertified;
						}
						else if (opt == "CLIP") {
							localCull = true;
						}
						else if (opt == "NOCLIP") {
							localCull = false;
						}
						else if (opt == "CCW") {
							if (accumInvert) {
								localWinding = Wind::CW;
							}
							else {
								localWinding = Wind::CCW;
							}
						}
						else if (opt == "CW") {
							if (accumInvert) {
								localWinding = Wind::CCW;
							}
							else {
								localWinding = Wind::CW;
							}
						}
						else if (opt == "INVERTNEXT") {
							invertNext = true;
						}
					}
				}
			}
			else if (lineType == 1) {
				if (maxReferenceDepth <= 0) {
					UE_LOG(LogTemp, Warning, TEXT("Exceeded max reference depth in '%s':%d"), *name, lineNum);
					continue;
				}

				int curColor;
				if (!(lineStream >> curColor)) {
					UE_LOG(LogTemp, Warning, TEXT("Missing subfile color in '%s':%d"), *name, lineNum);
					continue;
				}

				if (curColor == 16 || curColor == 24) {
					curColor = accumColor;
				}

				// Grab transformation matrix.
				buf.clear();
				buf.reserve(12);

				float val;
				for (size_t i = 0; i < 12 && lineStream >> val; ++i) {
					buf.push_back(val);
				}

				if (buf.size() != 12) {
					UE_LOG(LogTemp, Warning, TEXT("Missing subfile transformation matrix values in '%s':%d"), *name, lineNum);
					continue;
				}

				const FMatrix curTransform(
					FVector(buf[3], buf[6], buf[9]),
					FVector(buf[4], buf[7], buf[10]),
					FVector(buf[5], buf[8], buf[11]),
					FVector(buf[0], buf[1], buf[2]));

				std::string subFile;
				if (!(lineStream >> subFile)) {
					UE_LOG(LogTemp, Warning, TEXT("Missing subfile path in '%s':%d"), *name, lineNum);
					continue;
				}

				// If certification unknown, then implicitly uncertify.
				if (localCertified == Certify::Unknown) {
					localCertified = Certify::Uncertified;
				}

				if (localCertified == Certify::Certified) {
					_ImportLdrawFile(ldrawRoot, UTF8_TO_TCHAR(subFile.c_str()), curTransform * accumTransform, accumCull & localCull, accumInvert ^ invertCur, curColor, maxReferenceDepth - 1, singleSidedGroup, doubleSidedGroup, desc);
				}
				else {
					_ImportLdrawFile(ldrawRoot, UTF8_TO_TCHAR(subFile.c_str()), curTransform, false, accumInvert ^ invertCur, curColor, maxReferenceDepth - 1, singleSidedGroup, doubleSidedGroup, desc);
				}
			}
			else if (lineType == 3 || lineType == 4) {
				int curColor;
				if (!(lineStream >> curColor)) {
					UE_LOG(LogTemp, Warning, TEXT("Missing subfile color in '%s':%d"), *name, lineNum);
					continue;
				}

				if (curColor == 16 || curColor == 24) {
					curColor = accumColor;
				}

				// Grab 3 or 4 xyz-points.
				const size_t numPoints = lineType;
				buf.clear();
				buf.reserve(numPoints * 3);

				float val;
				for (size_t i = 0; i < numPoints * 3 && lineStream >> val; ++i) {
					buf.push_back(val);
				}

				if (buf.size() != numPoints * 3) {
					UE_LOG(LogTemp, Warning, TEXT("Missing point positions in '%s':%d"), *name, lineNum);
					continue;
				}

				// If certification unknown, then implicitly uncertify.
				if (localCertified == Certify::Unknown) {
					localCertified = Certify::Uncertified;
				}

				{
					desc->ReserveNewVertices(4);

					Wind effectiveWind = localWinding;
					if (accumTransform.Determinant() < 0.0f) {
						effectiveWind = effectiveWind == Wind::CW ? Wind::CCW : Wind::CW;
					}

					FVertexID vertexIds[4];
					for (size_t i = 0; i < numPoints; ++i) {
						vertexIds[i] = desc->CreateVertex();
						FVector v = accumTransform.TransformPosition(
							FVector(buf[i * 3 + 0], buf[i * 3 + 1], buf[i * 3 + 2]));
						vertexPositions[vertexIds[i]] = FVector(v.X, -v.Z, -v.Y);

					}

					desc->ReserveNewVertexInstances(4);

					TArray<FVertexInstanceID> vertexInstanceIds;
					vertexInstanceIds.Init(FVertexInstanceID(), numPoints);
					for (size_t i = 0; i < numPoints; ++i) {
						vertexInstanceIds[i] = desc->CreateVertexInstance(vertexIds[i]);
					}

					desc->ReserveNewPolygons(1);
					FPolygonID polygonID;
					if (accumCull && localCull && localCertified == Certify::Certified) {
						// Draw with backface culling.
						polygonID = desc->CreatePolygon(singleSidedGroup, vertexInstanceIds);
					}
					else {
						// Draw double-sided.
						UE_LOG(LogTemp, Warning, TEXT("Double-sided drawing currently unsupported in '%s':%d"), *name, lineNum);
						polygonID = desc->CreatePolygon(doubleSidedGroup, vertexInstanceIds);
					}

					if (effectiveWind == Wind::CW) {
						desc->ReversePolygonFacing(polygonID);
					}
				}
			}
		}
	}

	void _AssignMaterial(UStaticMesh* mesh, const FPolygonGroupID& groupID, UMaterial* material)
	{
		uint32 slotIndex = groupID.GetValue();
		FStaticMaterial StaticMaterial(material, *LexToString(slotIndex));
		if (!mesh->StaticMaterials.IsValidIndex(slotIndex))
		{
			mesh->StaticMaterials.Add(MoveTemp(StaticMaterial));
		}
		else if (!(mesh->StaticMaterials[slotIndex] == StaticMaterial))
		{
			mesh->StaticMaterials[slotIndex] = MoveTemp(StaticMaterial);
		}

		FMeshSectionInfo MeshSectionInfo;
		MeshSectionInfo.MaterialIndex = slotIndex;

		mesh->GetSectionInfoMap().Set(/*lod*/ 0, slotIndex, MeshSectionInfo);
		mesh->GetOriginalSectionInfoMap().CopyFrom(mesh->GetSectionInfoMap());
	}

} // anonymous namespace

UStaticMesh* ABasePlayerController::GetLdrawMesh(const FString& name)
{
	if (UStaticMesh * *mesh = _cache.Find(name)) {
		return *mesh;
	}

	FString ldrawRoot = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Ldraw"));

	UStaticMesh* mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	mesh->AddSourceModel();

	FMeshDescription* desc = mesh->CreateMeshDescription(0);
	FPolygonGroupID singleSidedGroup = desc->CreatePolygonGroup();
	FPolygonGroupID doubleSidedGroup = desc->CreatePolygonGroup();

	_ImportLdrawFile(ldrawRoot, name, FMatrix::Identity, true, false, 42, 5, singleSidedGroup, doubleSidedGroup, desc);

	// XXX: use polygon group 0 for single-sided, 1 for double-sided
	if (SingleSidedMaterial) {
		_AssignMaterial(mesh, singleSidedGroup, SingleSidedMaterial);
	}
	if (DoubleSidedMaterial) {
		_AssignMaterial(mesh, doubleSidedGroup, DoubleSidedMaterial);
	}

	FStaticMeshSourceModel& srcModel = mesh->GetSourceModel(0);
	srcModel.BuildSettings.bGenerateLightmapUVs = false;
	srcModel.BuildSettings.bComputeWeightedNormals = false;
	srcModel.BuildSettings.bRecomputeNormals = true;
	srcModel.BuildSettings.bRecomputeTangents = true;
	srcModel.BuildSettings.bUseMikkTSpace = false;
	srcModel.BuildSettings.bBuildAdjacencyBuffer = false;

	mesh->CommitMeshDescription(0);
	mesh->CreateBodySetup();
	mesh->SetLightingGuid();
	mesh->PostEditChange();

	_cache.Add(name, mesh);
	return mesh;
}

TArray<FLdrawPartInfo> ABasePlayerController::GetLdrawPartList()
{
	FString ldrawRoot = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Ldraw"));
	FString partsList = FPaths::Combine(ldrawRoot, TEXT("parts.lst"));
	if (!FPaths::FileExists(partsList)) {
		UE_LOG(LogTemp, Warning, TEXT("Could not find parts list!"));
		return {};
	}

	FString contents;
	FFileHelper::LoadFileToString(contents, *partsList);

	std::string outputString(TCHAR_TO_UTF8(*contents));
	std::stringstream sstream(outputString);

	TArray<FLdrawPartInfo> ret;

	int lineNum = 1;
	for (std::string line; std::getline(sstream, line); lineNum++) {
		int n = line.find(' ');
		std::string name = line.substr(0, n);

		std::string desc;
		if (n != std::string::npos) {
			// find string starting at non-space
			int m = line.find_first_not_of(' ', n);
			if (m != std::string::npos) {
				// and ending right before the newline
				int p = line.find_first_of("\r\n", m);
				if (p == std::string::npos) {
					p = line.size();
				}

				desc = line.substr(m, p-m);
			}
		}

		if (desc.empty() || !(desc.front() >= 'A' && desc.front() <= 'Z')) {
			continue;
		}

		FLdrawPartInfo pi;
		pi.Path = UTF8_TO_TCHAR(name.c_str());
		pi.Description = UTF8_TO_TCHAR(desc.c_str());
		ret.Add(pi);
	}

	UE_LOG(LogTemp, Warning, TEXT("Read parts list with %d entries"), ret.Num());
	return ret;
}

class FRuntimeToolsContextQueriesImpl : public IToolsContextQueriesAPI
{
public:
	FRuntimeToolsContextQueriesImpl(UInteractiveToolsContext* InContext, UWorld* InWorld)
	{
		ToolsContext = InContext;
		TargetWorld = InWorld;
	}

	virtual void SetContextActor(AActor* ContextActorIn)
	{
		ContextActor = ContextActorIn;
	}

	virtual void GetCurrentSelectionState(FToolBuilderState& StateOut) const override
	{
		StateOut.ToolManager = ToolsContext->ToolManager;
		StateOut.GizmoManager = ToolsContext->GizmoManager;
		StateOut.World = TargetWorld;
	}

	virtual void GetCurrentViewState(FViewCameraState& StateOut) const override
	{
		if (!ContextActor)
		{
			return;
		}

		bool bHasCamera = ContextActor->HasActiveCameraComponent();

		FVector Location;
		FRotator Rotation;
		ContextActor->GetActorEyesViewPoint(Location, Rotation);

		StateOut.Position = Location;
		StateOut.Orientation = Rotation.Quaternion();
		StateOut.HorizontalFOVDegrees = 90;
		StateOut.OrthoWorldCoordinateWidth = 1;
		StateOut.AspectRatio = 1.0;
		StateOut.bIsOrthographic = false;
		StateOut.bIsVR = false;
	}

	virtual EToolContextCoordinateSystem GetCurrentCoordinateSystem() const override
	{
		return EToolContextCoordinateSystem::Local;
	}

	virtual bool ExecuteSceneSnapQuery(const FSceneSnapQueryRequest& Request, TArray<FSceneSnapQueryResult>& Results) const override
	{
		return false;
	}

	virtual UMaterialInterface* GetStandardMaterial(EStandardToolContextMaterials MaterialType) const override
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

#if WITH_EDITOR
	virtual HHitProxy* GetHitProxy(int32 X, int32 Y) const override
	{
		return nullptr;
	}
#endif

protected:
	UInteractiveToolsContext* ToolsContext;
	UWorld* TargetWorld;
	AActor* ContextActor = nullptr;
};

class FRuntimeToolsContextTransactionImpl : public IToolsContextTransactionsAPI
{
public:
	virtual void DisplayMessage(const FText& Message, EToolMessageLevel Level) override
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToolMessage] %s"), *Message.ToString());
	}

	virtual void PostInvalidation() override
	{
	}

	virtual void BeginUndoTransaction(const FText& Description) override
	{
	}

	virtual void EndUndoTransaction() override
	{
	}

	virtual void AppendChange(UObject* TargetObject, TUniquePtr<FToolCommandChange> Change, const FText& Description) override
	{
	}

	virtual bool RequestSelectionChange(const FSelectedOjectsChangeList& SelectionChange) override
	{
		return false;
	}
};


void ABasePlayerController::BeginPlay()
{
	APlayerController::BeginPlay();
	ToolsContext = NewObject<UInteractiveToolsContext>();
	_contextQueriesAPI = MakeShared<FRuntimeToolsContextQueriesImpl>(ToolsContext, GetWorld());
	_contextTransactionsAPI = MakeShared<FRuntimeToolsContextTransactionImpl>();
	_contextQueriesAPI->SetContextActor(GetPawn());
	ToolsContext->Initialize(_contextQueriesAPI.Get(), _contextTransactionsAPI.Get());
}

void ABasePlayerController::ShowTransformGizmo(AActor* controlled)
{
	if (!ToolsContext) {
		return;
	}

	if (!ToolsContext->GizmoManager) {
		return;
	}
	
	auto TransformProxy = NewObject<UTransformProxy>(this);
	TransformProxy->AddComponent(controlled->GetRootComponent());

	ToolsContext->GizmoManager->DestroyAllGizmosByOwner(this);

	auto TransformGizmo = ToolsContext->GizmoManager->CreateCustomTransformGizmo(ETransformGizmoSubElements::TranslateRotateUniformScale, this);
	TransformGizmo->SetActiveTarget(TransformProxy);
}

void ABasePlayerController::HideAllGizmos()
{
	if (!ToolsContext) {
		return;
	}

	if (!ToolsContext->GizmoManager) {
		return;
	}

	ToolsContext->GizmoManager->DestroyAllGizmosByOwner(this);
}

void ABasePlayerController::Tick(float DeltaTime)
{
	APlayerController::Tick(DeltaTime);

	{
		FInputDeviceState InputState = CurrentMouseState;
		InputState.InputDevice = EInputDevices::Mouse;

		FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();
		FVector2D LastMousePosition = FSlateApplication::Get().GetLastCursorPos();
		FModifierKeysState ModifierState = FSlateApplication::Get().GetModifierKeys();

		UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
		TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
		FGeometry ViewportGeometry;
		if (ensure(LayerManager.IsValid()))
		{
			ViewportGeometry = LayerManager->GetViewportWidgetHostGeometry();
		}
		// why do we need this scale here? what is it for?
		FVector2D ViewportMousePos = ViewportGeometry.Scale * ViewportGeometry.AbsoluteToLocal(MousePosition);


		// update modifier keys
		InputState.SetModifierKeyStates(
			ModifierState.IsLeftShiftDown(),
			ModifierState.IsAltDown(),
			ModifierState.IsControlDown(),
			ModifierState.IsCommandDown());

		if (ViewportClient)
		{
			FSceneViewport* Viewport = ViewportClient->GetGameViewport();

			FEngineShowFlags* ShowFlags = ViewportClient->GetEngineShowFlags();
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
				ViewportClient->Viewport,
				GetWorld()->Scene,
				*ShowFlags)
				.SetRealtimeUpdate(true));

			ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
			FVector ViewLocation;
			FRotator ViewRotation;
			FSceneView* SceneView = LocalPlayer->CalcSceneView(&ViewFamily,  /*out*/ ViewLocation, /*out*/ ViewRotation, LocalPlayer->ViewportClient->Viewport);
			if (SceneView == nullptr)
			{
				return;		// abort abort
			}

			FVector4 ScreenPos = SceneView->PixelToScreen(ViewportMousePos.X, ViewportMousePos.Y, 0);
			UE_LOG(LogTemp, Warning, TEXT("pos '%f %f'"), ScreenPos.X, ScreenPos.Y);

			const FMatrix InvViewMatrix = SceneView->ViewMatrices.GetInvViewMatrix();
			const FMatrix InvProjMatrix = SceneView->ViewMatrices.GetInvProjectionMatrix();

			const float ScreenX = ScreenPos.X;
			const float ScreenY = ScreenPos.Y;

			FVector Origin;
			FVector Direction;
			if (!ViewportClient->IsOrtho())
			{
				Origin = SceneView->ViewMatrices.GetViewOrigin();
				Direction = InvViewMatrix.TransformVector(FVector(InvProjMatrix.TransformFVector4(FVector4(ScreenX * GNearClippingPlane, ScreenY * GNearClippingPlane, 0.0f, GNearClippingPlane)))).GetSafeNormal();
			}
			else
			{
				Origin = InvViewMatrix.TransformFVector4(InvProjMatrix.TransformFVector4(FVector4(ScreenX, ScreenY, 0.5f, 1.0f)));
				Direction = InvViewMatrix.TransformVector(FVector(0, 0, 1)).GetSafeNormal();
			}

			// fudge factor so we don't hit actor...
			Origin += 1.0 * Direction;

			InputState.Mouse.Position2D = FVector2D(ScreenX, ScreenY);
			InputState.Mouse.Delta2D = CurrentMouseState.Mouse.Position2D - PrevMousePosition;
			PrevMousePosition = InputState.Mouse.Position2D;
			InputState.Mouse.WorldRay = FRay(Origin, Direction);

			ToolsContext->InputRouter->PostHoverInputEvent(InputState);
		}
	}
}