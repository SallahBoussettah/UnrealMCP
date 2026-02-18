#include "Commands/MCPActorCommands.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorLevelLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "ScopedTransaction.h"

namespace ActorCommandsLocal
{
static UWorld* GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

static AActor* FindActorByName(const FString& ActorName)
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
		{
			return *It;
		}
	}
	return nullptr;
}

static TSharedPtr<FJsonObject> ActorToJson(AActor* Actor)
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
	Info->SetStringField(TEXT("name"), Actor->GetActorLabel());
	Info->SetStringField(TEXT("internal_name"), Actor->GetName());
	Info->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	FVector Loc = Actor->GetActorLocation();
	FRotator Rot = Actor->GetActorRotation();
	FVector Scale = Actor->GetActorScale3D();

	TArray<TSharedPtr<FJsonValue>> LocArr = {
		MakeShared<FJsonValueNumber>(Loc.X),
		MakeShared<FJsonValueNumber>(Loc.Y),
		MakeShared<FJsonValueNumber>(Loc.Z)
	};
	Info->SetArrayField(TEXT("location"), LocArr);

	TArray<TSharedPtr<FJsonValue>> RotArr = {
		MakeShared<FJsonValueNumber>(Rot.Pitch),
		MakeShared<FJsonValueNumber>(Rot.Yaw),
		MakeShared<FJsonValueNumber>(Rot.Roll)
	};
	Info->SetArrayField(TEXT("rotation"), RotArr);

	TArray<TSharedPtr<FJsonValue>> ScaleArr = {
		MakeShared<FJsonValueNumber>(Scale.X),
		MakeShared<FJsonValueNumber>(Scale.Y),
		MakeShared<FJsonValueNumber>(Scale.Z)
	};
	Info->SetArrayField(TEXT("scale"), ScaleArr);

	return Info;
}
} // namespace ActorCommandsLocal

using namespace ActorCommandsLocal;

// --- Spawn Actor ---
TSharedPtr<FJsonObject> FMCPSpawnActorCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ActorClassName = Params->GetStringField(TEXT("actor_class"));

	const TArray<TSharedPtr<FJsonValue>>* LocArr;
	FVector Location = FVector::ZeroVector;
	if (Params->TryGetArrayField(TEXT("location"), LocArr) && LocArr->Num() >= 3)
	{
		Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(), (*LocArr)[2]->AsNumber());
	}

	const TArray<TSharedPtr<FJsonValue>>* RotArr;
	FRotator Rotation = FRotator::ZeroRotator;
	if (Params->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr->Num() >= 3)
	{
		Rotation = FRotator((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(), (*RotArr)[2]->AsNumber());
	}

	const TArray<TSharedPtr<FJsonValue>>* ScaleArr;
	FVector Scale = FVector::OneVector;
	if (Params->TryGetArrayField(TEXT("scale"), ScaleArr) && ScaleArr->Num() >= 3)
	{
		Scale = FVector((*ScaleArr)[0]->AsNumber(), (*ScaleArr)[1]->AsNumber(), (*ScaleArr)[2]->AsNumber());
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Spawn Actor")));

	AActor* NewActor = nullptr;

	if (!BlueprintPath.IsEmpty())
	{
		UBlueprint* BP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
		if (!BP || !BP->GeneratedClass)
		{
			return ErrorResponse(FString::Printf(TEXT("Blueprint not found or not compiled: %s"), *BlueprintPath));
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NewActor = World->SpawnActor<AActor>(BP->GeneratedClass, Location, Rotation, SpawnParams);
	}
	else
	{
		UClass* ActorClass = nullptr;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(AActor::StaticClass()) &&
				(It->GetName() == ActorClassName || It->GetName() == FString::Printf(TEXT("A%s"), *ActorClassName)))
			{
				ActorClass = *It;
				break;
			}
		}

		if (!ActorClass)
		{
			return ErrorResponse(FString::Printf(TEXT("Actor class not found: %s"), *ActorClassName));
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	}

	if (!NewActor)
	{
		return ErrorResponse(TEXT("Failed to spawn actor"));
	}

	NewActor->SetActorScale3D(Scale);

	FString RequestedName = Params->GetStringField(TEXT("name"));
	if (!RequestedName.IsEmpty())
	{
		NewActor->SetActorLabel(RequestedName);
	}

	return SuccessResponse(ActorToJson(NewActor));
}

// --- Delete Actor ---
TSharedPtr<FJsonObject> FMCPDeleteActorCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return ErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Delete Actor")));
	Actor->GetWorld()->EditorDestroyActor(Actor, true);

	return SuccessResponse(FString::Printf(TEXT("Deleted actor: %s"), *ActorName));
}

// --- Set Actor Transform ---
TSharedPtr<FJsonObject> FMCPSetActorTransformCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return ErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Transform")));

	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (Params->TryGetArrayField(TEXT("location"), Arr) && Arr->Num() >= 3)
	{
		Actor->SetActorLocation(FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
	}
	if (Params->TryGetArrayField(TEXT("rotation"), Arr) && Arr->Num() >= 3)
	{
		Actor->SetActorRotation(FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
	}
	if (Params->TryGetArrayField(TEXT("scale"), Arr) && Arr->Num() >= 3)
	{
		Actor->SetActorScale3D(FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
	}

	return SuccessResponse(ActorToJson(Actor));
}

// --- Get Actors In Level ---
TSharedPtr<FJsonObject> FMCPGetActorsInLevelCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	FString ClassFilter = Params->GetStringField(TEXT("class_filter"));
	FString NameFilter = Params->GetStringField(TEXT("name_filter"));

	TArray<TSharedPtr<FJsonValue>> Actors;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		if (!ClassFilter.IsEmpty() && !Actor->GetClass()->GetName().Contains(ClassFilter))
			continue;
		if (!NameFilter.IsEmpty() && !Actor->GetActorLabel().Contains(NameFilter) && !Actor->GetName().Contains(NameFilter))
			continue;

		Actors.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
	}

	return SuccessResponse(Actors);
}

// --- Find Actors ---
TSharedPtr<FJsonObject> FMCPFindActorsCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString Query = Params->GetStringField(TEXT("query"));

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return ErrorResponse(TEXT("No editor world available"));
	}

	TArray<TSharedPtr<FJsonValue>> Results;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetActorLabel().Contains(Query) || Actor->GetName().Contains(Query))
		{
			Results.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
		}
	}

	return SuccessResponse(Results);
}

// --- Duplicate Actor ---
TSharedPtr<FJsonObject> FMCPDuplicateActorCommand::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));

	AActor* SourceActor = FindActorByName(ActorName);
	if (!SourceActor)
	{
		return ErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Duplicate Actor")));

	// Select the actor and duplicate via editor
	GEditor->SelectNone(true, true, false);
	GEditor->SelectActor(SourceActor, true, true);

	GEditor->edactDuplicateSelected(SourceActor->GetLevel(), false);

	// Find the new actor (most recently selected)
	AActor* NewActor = nullptr;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		NewActor = Cast<AActor>(*It);
		if (NewActor && NewActor != SourceActor)
			break;
	}

	if (!NewActor)
	{
		return ErrorResponse(TEXT("Failed to duplicate actor"));
	}

	const TArray<TSharedPtr<FJsonValue>>* OffsetArr;
	if (Params->TryGetArrayField(TEXT("location_offset"), OffsetArr) && OffsetArr->Num() >= 3)
	{
		FVector Offset((*OffsetArr)[0]->AsNumber(), (*OffsetArr)[1]->AsNumber(), (*OffsetArr)[2]->AsNumber());
		NewActor->SetActorLocation(SourceActor->GetActorLocation() + Offset);
	}

	FString NewName = Params->GetStringField(TEXT("new_name"));
	if (!NewName.IsEmpty())
	{
		NewActor->SetActorLabel(NewName);
	}

	return SuccessResponse(ActorToJson(NewActor));
}
