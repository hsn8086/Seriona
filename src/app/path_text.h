#pragma once

// 前端路径文本不变量：与后端边界交换的路径文本一律为 UTF-8。
// Windows 上 std::filesystem::path::string()/generic_string() 按 ANSI 代码页
// 转换，遇到不可表示字符会抛异常或产生乱码，路径文本通道禁止使用；
// QString::fromStdString / toStdString 的编码正是 UTF-8，与本 helper 配套。

#include <filesystem>
#include <string>

inline std::string pathTextUtf8(const std::filesystem::path &path)
{
    const auto utf8 = path.generic_u8string();
    return std::string{reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

inline std::filesystem::path pathFromUtf8(const std::string &text)
{
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(text.data()), text.size()}};
}
