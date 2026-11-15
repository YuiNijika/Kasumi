#pragma once

#include <memory>
#include <vector>

#include <kasumi/plugins/ksm_plugin.h>

std::vector<std::shared_ptr<KSMPlugin>> KasumiAppPlugins();
