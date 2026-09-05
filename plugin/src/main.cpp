#include "WidgetHost.h"

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm);

namespace
{
	void SetupLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			return;
		}
		*path /= "iWantWidgetsPrisma.log"sv;

#ifdef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#else
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#endif
		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
	}

	void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
	{
		switch (message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			// PrismaUI.dll is guaranteed loaded by now; acquire the API and
			// create the (session-lifetime) view.
			WidgetHost::Get().OnDataLoaded();
			break;
		}
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	SetupLog();

	SKSE::Init(a_skse);

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener("SKSE", SKSEMessageHandler)) {
		logger::critical("Failed to register SKSE messaging listener");
		return false;
	}

	auto* papyrus = SKSE::GetPapyrusInterface();
	if (!papyrus || !papyrus->Register(RegisterPapyrusFunctions)) {
		logger::critical("Failed to register Papyrus natives");
		return false;
	}

	logger::info("iWantWidgetsPrisma loaded");
	return true;
}
