// SPDX-FileCopyrightText: 2025 NTY.studio

#include "UnicodeBrowser.h"

#include "ToolMenus.h"
#include "UnicodeBrowserCommands.h"
#include "UnicodeBrowserStyle.h"

#include "Framework/Application/SlateApplication.h"

#include "UnicodeBrowser/UnicodeBrowserWidget.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/SListView.h"

static FName const UnicodeBrowserTabName("UnicodeBrowser");

#define LOCTEXT_NAMESPACE "FUnicodeBrowserModule"

void FUnicodeBrowserModule::StartupModule()
{
	FUnicodeBrowserStyle::Initialize();
	FUnicodeBrowserStyle::ReloadTextures();

	FUnicodeBrowserCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FUnicodeBrowserCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FUnicodeBrowserModule::PluginButtonClicked),
		FCanExecuteAction()
	);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnicodeBrowserModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UnicodeBrowserTabName, FOnSpawnTab::CreateRaw(this, &FUnicodeBrowserModule::OnSpawnPluginTab))
	                        .SetIcon(FSlateIcon(FUnicodeBrowserStyle::GetStyleSetName(), "UnicodeBrowser.OpenPluginWindow"))
	                        .SetDisplayName(LOCTEXT("FUnicodeBrowserTabTitle", "Unicode Browser"))
	                        .SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FUnicodeBrowserModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FUnicodeBrowserStyle::Shutdown();

	FUnicodeBrowserCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UnicodeBrowserTabName);
}

TSharedRef<SDockTab> FUnicodeBrowserModule::OnSpawnPluginTab(FSpawnTabArgs const& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SUnicodeBrowserWidget)			
		];
}

void FUnicodeBrowserModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(UnicodeBrowserTabName);
}

void FUnicodeBrowserModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	// register to all Menu => Window => Tools => Unicode Browser
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Window");
		{
			if (FToolMenuSection* Section = &Menu->FindOrAddSection("Tools"))
			{
				if (Section->Label.Get(FText::GetEmpty()).IsEmpty()) Section->Label = INVTEXT("TOOLS");
				Section->AddMenuEntryWithCommandList(FUnicodeBrowserCommands::Get().OpenPluginWindow, PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnicodeBrowserModule, UnicodeBrowser)
