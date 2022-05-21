#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CNS_SetExposureCompensation.generated.h"

class ACPlayerCameraManager;
class ACharacter;
class UAnimInstance;
/**
 * 
 */
UCLASS()
class CPP_POFOL_API UCNS_SetExposureCompensation : public UAnimNotifyState
{
	GENERATED_BODY()
	virtual void NotifyBegin(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float TotalDuration);
	virtual void NotifyTick(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float FrameDeltaTime);
	virtual void NotifyEnd(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation);

	ACharacter * owner;
	ACPlayerCameraManager * CameraManager;
	UAnimInstance * animInstance;
};
