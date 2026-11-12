#include "MCP/PBMCPApprovals.h"

#include "Containers/Ticker.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ProtoBoyMCPApprovals"

namespace ProtoBoyMCPApprovals
{
	/** One pending decision, shared by the buttons and the timeout. */
	struct FPending
	{
		TFunction<void(bool)> OnDecision;
		TWeakPtr<SNotificationItem> Notification;
		FTSTicker::FDelegateHandle TimeoutHandle;
		bool bDecided = false;

		void Decide(bool bApproved)
		{
			if (bDecided)
			{
				return;
			}
			bDecided = true;

			if (TimeoutHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
				TimeoutHandle.Reset();
			}
			if (const TSharedPtr<SNotificationItem> Item = Notification.Pin())
			{
				Item->SetText(bApproved ? LOCTEXT("Allowed", "Agent change allowed") : LOCTEXT("Denied", "Agent change denied"));
				Item->SetCompletionState(bApproved ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
				Item->SetExpireDuration(2.f);
				Item->ExpireAndFadeout();
			}

			TFunction<void(bool)> Callback = MoveTemp(OnDecision);
			Callback(bApproved);
		}
	};
}

void FProtoBoyMCPApprovals::Request(const FString& ClientName, const FText& Action, const FString& Details, TFunction<void(bool bApproved)> OnDecision)
{
	using ProtoBoyMCPApprovals::FPending;
	const TSharedRef<FPending> Pending = MakeShared<FPending>();
	Pending->OnDecision = MoveTemp(OnDecision);

	FNotificationInfo Info(FText::Format(LOCTEXT("Title", "{0} wants to: {1}"), FText::FromString(ClientName), Action));
	Info.SubText = FText::FromString(Details);
	Info.bFireAndForget = false;
	Info.bUseLargeFont = false;
	Info.bUseThrobber = true;
	Info.FadeOutDuration = 0.5f;
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("Allow", "Allow"),
		LOCTEXT("AllowTip", "Let the agent make this change."),
		FSimpleDelegate::CreateLambda([Pending]() { Pending->Decide(true); }),
		SNotificationItem::CS_Pending));
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("Deny", "Deny"),
		LOCTEXT("DenyTip", "Refuse; the agent is told the change was denied."),
		FSimpleDelegate::CreateLambda([Pending]() { Pending->Decide(false); }),
		SNotificationItem::CS_Pending));

	const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (!Item.IsValid())
	{
		// Notifications unavailable (e.g. headless): fail safe.
		Pending->Decide(false);
		return;
	}
	Item->SetCompletionState(SNotificationItem::CS_Pending);
	Pending->Notification = Item;

	Pending->TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Pending](float)
	{
		Pending->TimeoutHandle.Reset();
		Pending->Decide(false);
		return false;
	}), TimeoutSeconds);
}

#undef LOCTEXT_NAMESPACE
