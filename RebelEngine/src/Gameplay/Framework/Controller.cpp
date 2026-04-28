#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/Controller.h"
#include "Engine/Gameplay/Framework/Pawn.h"

DEFINE_LOG_CATEGORY(controllerLog)

Controller::Controller()
{
	SetTickGroup(ActorTickGroup::PrePhysics);
}

Controller::~Controller()
{
}

void Controller::BeginPlay()
{
	Actor::BeginPlay();
}

void Controller::EndPlay()
{
	Actor::EndPlay();
}

void Controller::Possess(Pawn* pawn)
{
	if (pawn == nullptr)
	{
		RB_LOG(controllerLog, warn, "Controller::Possess called with null pawn. Controller={} currentPawn={}",
			(void*)this, (void*)m_Pawn)
		UnPossess();
		return;
	}

	if (m_Pawn == pawn && pawn->GetController() == this)
	{
		RB_LOG(controllerLog, trace, "Controller::Possess ignored (already possessing). Controller={} Pawn={}",
			(void*)this, (void*)pawn)
		return;
	}

	if (m_Pawn != nullptr && m_Pawn != pawn)
		UnPossess();

	Controller* previousController = pawn->GetController();
	if (previousController != nullptr && previousController != this)
		previousController->UnPossess();

	m_Pawn = pawn;
	if (pawn->GetController() != this)
		pawn->SetController(this);

	RB_LOG(controllerLog, info, "Controller::Possess Controller={} Pawn={}",
		(void*)this, (void*)m_Pawn)
}

void Controller::UnPossess()
{
	if (m_Pawn == nullptr)
	{
		RB_LOG(controllerLog, trace, "Controller::UnPossess ignored (no pawn). Controller={}",
			(void*)this)
		return;
	}

	Pawn* oldPawn = m_Pawn;
	m_Pawn = nullptr;

	if (oldPawn->GetController() == this)
		oldPawn->SetController(nullptr);

	RB_LOG(controllerLog, info, "Controller::UnPossess Controller={} OldPawn={}",
		(void*)this, (void*)oldPawn)
}

void Controller::Tick(float dt)
{
	/*RB_LOG(controllerLog, trace, "Controller::Tick Controller={} Pawn={} dt={}",
		(void*)this, (void*)m_Pawn, dt)*/

	Actor::Tick(dt);
}

