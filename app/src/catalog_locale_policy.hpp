#pragma once
#include <string>

namespace nxnow::catalog
{
inline std::string Locale(const std::string& language)
{
    if (language == "fr") return "fr_FR";
    if (language == "es") return "es_ES";
    if (language == "it") return "it_IT";
    if (language == "pl") return "pl_PL";
    if (language == "ru") return "ru_RU";
    if (language == "uk") return "uk_UA";
    if (language == "zh-CN") return "zh_CN";
    return "en_US";
}
}
