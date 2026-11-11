#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogProtoBoy, Log, All);

class FProtoBoyModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static const FName DashboardTabName;

private:
	void RegisterMenus();
};
