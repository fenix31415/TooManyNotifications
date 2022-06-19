extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

void foo()
{
	auto ui = RE::UI::GetSingleton();
	if (!ui)
		return;

	auto menu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
	if (!menu.get())
		return;

	auto movie = menu->uiMovie.get();
	if (!movie)
		return;

	RE::GFxValue ShownCount, MAX_SHOWN, ShownMessageArray, _loc3_, _loc3_0;

	if (!movie->GetVariable(&ShownCount, "_root.HUDMovieBaseInstance.MessagesInstance.ShownCount") ||
		!movie->GetVariable(&MAX_SHOWN, "_global.Messages.MAX_SHOWN") ||
		!movie->GetVariable(&ShownMessageArray, "_root.HUDMovieBaseInstance.MessagesInstance.ShownMessageArray"))
		return;

	auto iShownCount = ShownCount.GetSInt();

	if (iShownCount == MAX_SHOWN.GetSInt()) {
		ShownMessageArray.Invoke<2>("splice", &_loc3_, { 0, 1 });
		_loc3_.GetElement(0, &_loc3_0);
		_loc3_0.Invoke("removeMovieClip");
		movie->SetVariable("_root.HUDMovieBaseInstance.MessagesInstance.ShownCount", iShownCount - 1, RE::GFxMovie::SetVarType::kNormal);
	}
}

class HUDMenuHook
{
public:
	static void Hook()
	{
		_ProcessMessage = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_HUDMenu[0])).write_vfunc(0x4, ProcessMessage);
	}

private:
	static RE::UI_MESSAGE_RESULTS ProcessMessage(void* self, RE::UIMessage& a_message)
	{
		if (a_message.type == RE::UI_MESSAGE_TYPE::kUpdate) {
			foo();
		}

		return _ProcessMessage(self, a_message);
	}

	static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		HUDMenuHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
