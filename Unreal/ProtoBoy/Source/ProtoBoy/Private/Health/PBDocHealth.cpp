#include "Health/PBDocHealth.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "PBSettings.h"

#define LOCTEXT_NAMESPACE "ProtoBoyDocHealth"

namespace ProtoBoyDocHealth
{
	bool IsRevisionControlConnected(FText& OutStatus)
	{
		ISourceControlModule& Module = ISourceControlModule::Get();
		if (!Module.IsEnabled())
		{
			OutStatus = LOCTEXT("NotConnected", "Not connected to revision control.");
			return false;
		}
		ISourceControlProvider& Provider = Module.GetProvider();
		if (!Provider.IsAvailable())
		{
			OutStatus = FText::Format(LOCTEXT("Unavailable", "{0} is set up but not available right now (offline or not logged in)."), FText::FromName(Provider.GetName()));
			return false;
		}
		OutStatus = FText::Format(LOCTEXT("Connected", "Connected to {0}."), FText::FromName(Provider.GetName()));
		return true;
	}

	bool IsEnabled()
	{
		FText Ignored;
		return GetDefault<UProtoBoySettings>()->bEnableDocHealthChecks && IsRevisionControlConnected(Ignored);
	}

	void OpenRevisionControlLogin()
	{
		ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
	}
}

#undef LOCTEXT_NAMESPACE
