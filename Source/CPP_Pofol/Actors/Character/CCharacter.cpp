#include "CCharacter.h"
#include "Components/CSkillManagerComponent.h"
#include "Components/CWeaponManagerComponent.h"
#include "Components/CParticleManagerComponent.h"
#include "Components/CReactionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Util/CLog.h"


// Sets default values
ACCharacter::ACCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	//Setting Components
	{
		weaponManagerComponent8 = CreateDefaultSubobject<UCWeaponManagerComponent>(TEXT("WeaponManagerComponent"));
		skillManagerComponent7 = CreateDefaultSubobject<UCSkillManagerComponent>(TEXT("SkillManagerComponent"));
		reactionComponent1 = CreateDefaultSubobject<UCReactionComponent>(TEXT("ReactionComponent"));
	}

	//Setting MovementComponent
	{
		GetCharacterMovement()->GravityScale = gravityScale;

		//current movement setting 커브저장
		static ConstructorHelpers::FObjectFinder<UCurveVector> MovementCurve(TEXT("CurveVector'/Game/Curves/Locomotion/movementSettings/movementCurve.movementCurve'"));
		currentMovementSettings.movementCurve = MovementCurve.Object;
		static ConstructorHelpers::FObjectFinder<UCurveFloat> RotationRateCurve(TEXT("CurveFloat'/Game/Curves/Locomotion/movementSettings/rotationRateCurve.rotationRateCurve'"));
		currentMovementSettings.rotationRateCurve = RotationRateCurve.Object;
	}

}

void ACCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	//Setting characterStructs
	{
		characterStructs23 = NewObject<UCCharacterStruct>(this, UCCharacterStruct::StaticClass(), TEXT("characterStructs"));
		lastRotationMode = GET_STATE(RotationMode);
	}

	//reactionComponent1 = NewObject<UCReactionComponent>(this, UCReactionComponent::StaticClass());
	//check(reactionComponent1);
	//reactionComponent1->RegisterComponent();//동적으로 만들때 사용한다.


	particleManagerComponent = NewObject<UCParticleManagerComponent>(this, UCParticleManagerComponent::StaticClass());
	check(particleManagerComponent);
	particleManagerComponent->RegisterComponent();

	animnInst = GetMesh()->GetAnimInstance();
}

void ACCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void ACCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	deltaTime = DeltaTime;


	UpdateSubState();
	UpdateEssentialValue();

	if (GET_STATE(SubState) == ESubState::LAY_DOWN)
	{
		UpdateRagDoll();
	}
	else
	{
		UpdateSetLocation();
		UpdateMovementSetting();
		UpdateRotation();
	}
	UpdateMovementPreviousValue();

}

void ACCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	if (OnJump.IsBound() == true) OnJump.Execute();
	/*SetLastRotationMode(GET_STATE(RotationMode));
	SET_STATE(RotationMode, Velocity);*/
	return;
}


void ACCharacter::Landed(const FHitResult & Hit)
{
	Super::Landed(Hit);

	if (characterStructs23 == nullptr) return;
	if (OnLand.IsBound() == true) OnLand.Execute();
	//PutBackLastRotationMode();
	SET_STATE(MainState, Ground);
}


void ACCharacter::Falling()
{
	Super::Falling();

	if (characterStructs23 == nullptr) return;

	//나중에 공중에서의 Rotation정의하기
	/*SetLastRotationMode(GET_STATE(RotationMode));
	SET_STATE(RotationMode, Velocity);*/
	SET_STATE(MainState, Air);
}


////////////////////////////////////////////////////////////////////////////////
//Character Info
void ACCharacter::GetCharacterState(
	EMainState& mainState_out, ESubState& subState_out, EMoveMode& movemode_out)
{
	if(characterStructs23 == nullptr) return;
	mainState_out = characterStructs23->GetMainState();
	subState_out = characterStructs23->GetSubState();
	movemode_out = characterStructs23->GetMoveMode();
	return;
}

////////////////////////////////////////////////////////////////////////////////
//Movement

void ACCharacter::UpdateSubState()
{
	if (characterStructs23 == nullptr) return;
	if (animnInst == nullptr) return;
	
	float hittedCurveValue = animnInst->GetCurveValue(FName("Hitted_Curve"));
	if (GET_STATE(SubState) == ESubState::NONE && 0.1f < hittedCurveValue)
	{
		SET_STATE(SubState, Hitted);
		return;
	}
	if (GET_STATE(SubState) == ESubState::HITTED )
	{
		if (1.1f < hittedCurveValue)
		{
			SET_STATE(SubState, LayDown);
			RagDollStart();
			return;
		}
		if(hittedCurveValue == 0.0f)
		{
			SET_STATE(SubState,None);
			return;
		}
	}
}

void ACCharacter::UpdateEssentialValue()
{
	currentVelocity = this->GetVelocity();
	acceleration = (currentVelocity - previousVelocity) / deltaTime;
	currentSpeed = currentVelocity.Size2D();

	if(1.0f < currentSpeed)
	{
		LastVelocityRotation = currentVelocity.Rotation();
	}

	if (0.0f < GetCharacterMovement()->GetCurrentAcceleration().Size())
	{
		LastMovementInputRotation = GetCharacterMovement()->GetCurrentAcceleration().Rotation();
	}

	aimYawRate = FMath::Abs((GetControlRotation().Yaw - previousAimYaw) / deltaTime);
}

void ACCharacter::UpdateGroundRotation()
{
	//상속클래스에서 기타 동작(SubState,...)에 의한 회전을 따로 추가하고싶을경우 이 함수를 오버라이드하세요
	if (AddUpdateGroundedRotation() == true) return;
}


void ACCharacter::UpdateRotation()
{
	//상속클래스에서 기타 동작(SubState,...)에 의한 회전을 따로 추가하고싶을경우 이 함수를 오버라이드하세요
	if (AddUpdateGroundedRotation() == true) return;
	
	if( currentSpeed <= 0.0f )
	{
		return;
	}

	//interpSpeed는 두 모드 모두 동일함
	float interpSpeed = currentMovementSettings.rotationRateCurve->GetFloatValue(GetMappedSpeed());
	FRotator resultRot = FRotator::ZeroRotator;

	if (characterStructs23 == nullptr) return;
	if (GET_STATE(RotationMode) == ERotationMode::NONE) { return; }
	if (GET_STATE(RotationMode) == ERotationMode::VELOCITY)
	{
		resultRot = SmoothCharacterRotation(LastVelocityRotation.Yaw, 800.0f, CalculateGroundedRotationRate());
	}
	if (GET_STATE(RotationMode) == ERotationMode::CONTROLLER)
	{
		resultRot = SmoothCharacterRotation(GetControlRotation().Yaw, 500.0f, CalculateGroundedRotationRate());
	}


	SetActorRotation(resultRot, ETeleportType::None);

	return;
}


void ACCharacter::UpdateMovementPreviousValue()
{
	previousVelocity = this->GetVelocity();
	previousAimYaw = GetControlRotation().Yaw;
	return;
}


void ACCharacter::UpdateMovementSetting()
{
	if (characterStructs23 == nullptr) return;
	if (GET_STATE(MainState) == EMainState::GROUND)
	{
		UpdateCurrentMovementSettings();
	}
	if (GET_STATE(MainState) == EMainState::AIR)
	{

	}
	
}

void ACCharacter::UpdateCurrentMovementSettings()
{
	/** || 각 모드에 따라  MaxWalkSpeed와 MaxFlySpeed를 세팅합니다. ||*/
	if(GET_STATE(MoveMode) == EMoveMode::WALKING)
	{
		GetCharacterMovement()->MaxWalkSpeed = currentMovementSettings.animWalkSpeed * multiplySpeed;
		GetCharacterMovement()->MaxFlySpeed = currentMovementSettings.animWalkSpeed * multiplySpeed;
	}
	if(GET_STATE(MoveMode) == EMoveMode::RUNNING)
	{
		GetCharacterMovement()->MaxWalkSpeed = currentMovementSettings.animRunSpeed * multiplySpeed;
		GetCharacterMovement()->MaxFlySpeed = currentMovementSettings.animRunSpeed * multiplySpeed;
	}


	/** || Acc, Dec, Ground Friction을 업데이트 합니다. ||
	 *	curveValue : 커브의 X축은 MaxAcc, Y축은 Dec, Z축은 Ground Friction의 값을 가지고 있습니다.
	 */
	FVector curveValue = currentMovementSettings.movementCurve->GetVectorValue(GetMappedSpeed());

	GetCharacterMovement()->MaxAcceleration = curveValue.X * multiplySpeed;
	GetCharacterMovement()->BrakingDecelerationWalking = curveValue.Y * multiplySpeed;

	if (GET_STATE(MainState) == EMainState::GROUND)
	{
		GetCharacterMovement()->GroundFriction = curveValue.Z;
	}
	else
	{
		GetCharacterMovement()->GroundFriction = 5.0f;
	}

	return;
}


float ACCharacter::GetMappedSpeed()
{
	FVector2D inRange = FVector2D::ZeroVector;
	FVector2D outRange = FVector2D::ZeroVector;

	//걷기상태일경우
	if (currentSpeed <= currentMovementSettings.animWalkSpeed* multiplySpeed)
	{
		inRange = { 0.0f, currentMovementSettings.animWalkSpeed* multiplySpeed };
		outRange = { 0.0f, 1.0f };
	}
	else //if (currentSpeed <= currentMovementSettings.animRunSpeed) 나중에 상태 추가될수도있음
	{
		inRange = { currentMovementSettings.animWalkSpeed* multiplySpeed, currentMovementSettings.animRunSpeed* multiplySpeed };
		outRange = { 1.0f, 2.0f };
	}

	return FMath::GetMappedRangeValueClamped(inRange, outRange, currentSpeed);
}

FRotator ACCharacter::SmoothCharacterRotation(float targetYaw, const float targetInterpSpeed, const float actorInterpSpeed)
{
	FRotator target = FRotator(0.0f, targetYaw, 0.0f);
	targetRotation = FMath::RInterpConstantTo(targetRotation, target, deltaTime, targetInterpSpeed);

	return FMath::RInterpTo(GetActorRotation(), targetRotation, deltaTime, actorInterpSpeed);
}

float ACCharacter::CalculateGroundedRotationRate()
{
	return currentMovementSettings.rotationRateCurve->GetFloatValue(GetMappedSpeed()) *
		UKismetMathLibrary::MapRangeClamped(aimYawRate, 0.0f, 300.0f, 1.0f, 2.0f);
}


void ACCharacter::GetCharacterMovementValue(
	FVector& location, FRotator& rotation, FVector& velocity, float& speed,
	float& animMaxWalkSpeed, float& animMaxRunSpeed, float& multiSpeed, FVector& acc, FVector& DistanceAcc)
{
	location = this->GetActorLocation();
	rotation = this->GetActorRotation();
	velocity = currentVelocity;
	speed = currentSpeed;
	animMaxWalkSpeed = currentMovementSettings.animWalkSpeed * multiplySpeed;
	animMaxRunSpeed = currentMovementSettings.animRunSpeed * multiplySpeed;
	multiSpeed = this->multiplySpeed;
	acc = GetCharacterMovement()->GetCurrentAcceleration();
	DistanceAcc = acceleration;
}


void ACCharacter::UpdateSetLocation()
{
	SetLocation();
}

bool ACCharacter::AddUpdateGroundedRotation()
{
	if (characterStructs23 == nullptr) return false;
	if (GET_STATE(SubState) == ESubState::ATTACK)
	{
		check(skillManagerComponent7);
		
		FRotator tempRot = FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, skillManagerComponent7->GetAttackYaw(), 0.0f), deltaTime, attackRotInterpSpeed);

		SetActorRotation(tempRot, ETeleportType::None);
		return true;
	}

	return false;
}

void ACCharacter::PutBackLastRotationMode()
{
	if (lastRotationMode == ERotationMode::VELOCITY)
	{
		SET_STATE(RotationMode,Velocity);
	}
	if (lastRotationMode == ERotationMode::CONTROLLER)
	{
		SET_STATE(RotationMode,Controller);
	}
}

void ACCharacter::SetLastRotationMode(ERotationMode mode)
{
	lastRotationMode = mode;
}

void ACCharacter::RagDollStart()
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_None);
	SET_STATE(SubState, LayDown);

	//GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName("pelvis"), true, true);
	//몽타주멈춤 실행? 
}

void ACCharacter::UpdateRagDoll()
{
	/**	|| 너무 부자연스럽게 꺾이지 않도록 속도에따라 단단하게 관절을 구성합니다 || */
	LastRagDollVelocity = GetMesh()->GetPhysicsLinearVelocity();
	
	if (LastRagDollVelocity.Size() < 1.5f)
	{
		bCanGetUp = true;
	}
	else
	{
		bCanGetUp = false;
	}

	float inSpring = UKismetMathLibrary::MapRangeClamped(
		LastRagDollVelocity.Size(),
		0.0f, 1000.0f,
		0.0f, 25000.0f
	);
	GetMesh()->SetAllMotorsAngularDriveParams(inSpring, 0.0f, 0.0f);

	/**	|| 캡슐을 메시에 붙입니다. || */
	RagDoll_SetCapusleLoc();
}

void ACCharacter::RagDoll_SetCapusleLoc()
{
	FVector TargetRagdolLoc = GetMesh()->GetSocketLocation(FName("pelvis"));
	FRotator TargetRagdolRot = GetMesh()->GetSocketRotation(FName("pelvis"));

	bIsRagdolFaceUp = TargetRagdolRot.Roll < 0.0f;
	if (bIsRagdolFaceUp == true)
	{///윗면을 향할때
		TargetRagdolRot.Yaw = TargetRagdolRot.Yaw - 180.0f;
	}
	TargetRagdolRot.Roll = 0.0f;
	TargetRagdolRot.Pitch = 0.0f;


	FHitResult hitResult;
	float capsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector endLoc = FVector(TargetRagdolLoc.X, TargetRagdolLoc.Y,
		TargetRagdolLoc.Z - capsuleHalfHeight);
	if (GetWorld()->LineTraceSingleByChannel(hitResult, TargetRagdolLoc, endLoc,
		ECollisionChannel::ECC_Visibility) == false)
	{
		bRagdollOnGround = false;
		SetActorLocationAndRotation(TargetRagdolLoc, TargetRagdolRot);
		return;
	}

	FVector setLoc;
	setLoc.X = TargetRagdolLoc.X;
	setLoc.Y = TargetRagdolLoc.Y;
	setLoc.Z = TargetRagdolLoc.Z + 2.0f +
		capsuleHalfHeight - FMath::Abs(hitResult.ImpactPoint.Z - TargetRagdolLoc.Z);
	
	bRagdollOnGround = true;
	float interpSpeed = 15.0f;
	
	SetActorLocationAndRotation(setLoc, TargetRagdolRot);
}

void ACCharacter::RagDollEnd()
{
	SET_STATE(SubState, None);
	/**	|| 자연스럽게 일어나기 위한 포즈 저장 || */
	GetMesh()->GetAnimInstance()->SavePoseSnapshot(FName("RagdollPose"));

	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);

	GetMesh()->GetAnimInstance()->Montage_Play(GetGetUpAnimaMontage(bIsRagdolFaceUp));

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionObjectType(ECollisionChannel::ECC_GameTraceChannel2);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	GetMesh()->SetAllBodiesSimulatePhysics(false);
}

//람다 + 타이머사용 레거시임
/*GetWorld()->GetTimerManager().SetTimer(timerHandle, FTimerDelegate::CreateLambda([&]()
{
	SetGround();
}), 0.1f, false);*/
