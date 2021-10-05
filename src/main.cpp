#include<symboli/carotene/core.hpp>
#include<optional>
#include<nlohmann/json.hpp>
#include<filesystem>
#include<cstdio>
#include<fstream>
#include<iostream>

static std::optional<symboli::carotene::core> core;

struct config_t{
	struct{
		bool request = false;
		bool response = false;
	}save;
	std::filesystem::path export_directory = "";
}static config;
static inline void from_json(const nlohmann::json& j, config_t& conf){
	auto config_opt_read = [](const nlohmann::json& j, const nlohmann::json::object_t::key_type& key, auto& value){
		core->config_read<true>("Symboli Carotene :: config_read", j, key, value);
	};
	if(j.contains("save") && j["save"].is_object()){
		config_opt_read(j["save"], "request", conf.save.request);
		config_opt_read(j["save"], "response", conf.save.response);
	}
	if(j.contains("export_directory") && j["export_directory"].is_string()){
		std::string str;
		j["export_directory"].get_to(str);
		conf.export_directory = str;
	}
}

static inline will::expected<void, std::error_code> write_file(std::filesystem::path path, const char* buffer, std::size_t len){
	::FILE* fp;
	auto ret = ::fopen_s(&fp, path.string().c_str(), "wb");
	if(ret)
		return will::make_unexpected(std::error_code{ret, std::generic_category()});
	std::unique_ptr<::FILE, decltype(&::fclose)> _{fp, &::fclose};
	::fwrite(buffer, 1, len, fp);
	return {};
}

static inline BOOL process_attach(HINSTANCE hinst){
	const std::filesystem::path plugin_path{will::get_module_file_name(hinst).value()};
	core =+ symboli::carotene::core::create(plugin_path.parent_path()/"symboli_carotene_core.dll");

	std::ifstream config_file{(plugin_path.parent_path()/plugin_path.stem()).concat(".config.json")};
	if(config_file.is_open())try{
		nlohmann::json j;
		config_file >> j;
		config = j.get<config_t>();
		if(config.export_directory.is_relative())
			config.export_directory = plugin_path.parent_path()/config.export_directory;
		const bool exists = std::filesystem::exists(config.export_directory);
		if(exists && !std::filesystem::is_directory(config.export_directory))
			throw std::runtime_error("Carotene Dump: " + config.export_directory.string() + " is not directory");
		if(!exists)
			std::filesystem::create_directories(config.export_directory);
		std::cout << "Carotene Dump config: \n"
			"  save: {.request: " << config.save.request << ", .response: " << config.save.response << "}\n"
			"  export_directory: " << config.export_directory << std::endl;
	}catch(std::exception& e){
		::MessageBoxA(nullptr, e.what(), "Carotene Dump exception", MB_OK|MB_ICONWARNING|MB_SETFOREGROUND);
	}

	if(config.save.request)
		core->add_request_func([](const std::vector<std::byte>& data){
			const auto current_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			write_file(config.export_directory/(current_time+"Q.msgpack"), reinterpret_cast<const char*>(data.data()), data.size());
		});
	if(config.save.response)
		core->add_response_func([](const std::vector<std::byte>& data){
			const auto current_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			write_file(config.export_directory/(current_time+"R.msgpack"), reinterpret_cast<const char*>(data.data()), data.size());
		});

	return TRUE;
}

static inline BOOL process_detach(){
	return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)try{
	switch(fdwReason){
	case DLL_PROCESS_ATTACH:
		return process_attach(hinstDLL);
	case DLL_PROCESS_DETACH:
		return process_detach();
	default:
		return TRUE;
	}
}catch(std::exception& e){
	::MessageBoxA(nullptr, e.what(), "Carotene Dump exception", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
	return FALSE;
}
