#pragma once

#include "bridge.hpp"
#include "json.hpp"
#include <string>

namespace realplus {

Json list_plus_tools();
Json dispatch_plus_tool(RealBridge& real, const std::string& name, const Json& args);

Json run_scac(RealBridge& real, const Json& args);
Json run_uiscan(RealBridge& real, const Json& args);
Json run_httpscan(RealBridge& real, const Json& args);
Json run_keyscan(RealBridge& real, const Json& args);
Json run_recon(RealBridge& real, const Json& args);

Json run_remmap(RealBridge& real, const Json& args);
Json run_remtrace(RealBridge& real, const Json& args);
Json run_remotesim(RealBridge& real, const Json& args);
Json run_clicheck(RealBridge& real, const Json& args);
Json run_modscan(RealBridge& real, const Json& args);
Json run_econscan(RealBridge& real, const Json& args);

Json run_proxscan(RealBridge& real, const Json& args);
Json run_npcscan(RealBridge& real, const Json& args);
Json run_zonescan(RealBridge& real, const Json& args);
Json run_toolscan(RealBridge& real, const Json& args);
Json run_netcheck(RealBridge& real, const Json& args);
Json run_animscan(RealBridge& real, const Json& args);
Json run_animmake(RealBridge& real, const Json& args);
Json run_scriptmap(RealBridge& real, const Json& args);
Json run_hookwatch(RealBridge& real, const Json& args);

Json run_web_scripts(RealBridge& real, const Json& args);
Json run_web_scripts_opt(RealBridge* real, const Json& args);

Json run_rbxgame_search(RealBridge& real, const Json& args);
Json run_rbxgame_search_opt(RealBridge* real, const Json& args);

} // namespace realplus
