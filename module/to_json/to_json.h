// to_json.h
#pragma once

/**
 * ************************************************************************
 *
 * @file to_json.h
 * @author 刘杰
 * @brief 基于 Boost.pfr、Jsoncpp 与 C++17折叠表达式,使 聚合POD 类型获得 '定义即序列化' 的能力，无需编写序列化接口，只出不进
 * json 与 STL 的对应关系
 *  json_array -> std::vector
 * ************************************************************************
 * ************************************************************************
 */
// 要求 C++ 版本 >= std=c++20
// Boost.pft, Version >= 1.88
#include <boost/pfr.hpp>
#include <boost/pfr/core_name.hpp>

// Jsoncpp,无缝集成至 Drogon 框架
#include <json/json.h>

// 标准库
#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <array>
#include <type_traits>
#include <fstream>

namespace module::to_json {
namespace pfr = boost::pfr;
namespace detail {
/**
 * ************************************************************************
 * @brief 内部处理函数,将 POD 的值赋值给 Json对象
 *
 * @param[in] json_val  json对象
 * @param[in] value  值
 *
 * ************************************************************************
 */
template <typename T>
void to_json_value(Json::Value &json_val, const T &value) {
    if constexpr (std::is_arithmetic_v<T>) {
        json_val = value;
    } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
        json_val = static_cast<std::string>(value);
    } else if constexpr (requires {
                             value.size();
                             value.begin();
                             value.end();
                         }) {
        json_val = Json::arrayValue;
        for (const auto &item : value) {
            Json::Value item_json;
            to_json_value(item_json, item);
            json_val.append(item_json);
        }
    } else {
        auto names = pfr::names_as_array<T>();
        auto values = pfr::structure_to_tuple(value);

        std::apply(
            [&json_val, &names](const auto &...fields) {
                size_t i = 0;
                (
                    [&json_val, &names,
                     &i](const auto &field) {
                        to_json_value(
                            json_val[std::string(
                                names[i++])],
                            field);
                    }(fields),
                    ...);
            },
            values);
    }
}
} // namespace detail

/**
 * ************************************************************************
 * @brief 核心接口,将 POD 序列化成 Json 对象
 *
 * @param[in] structure POD 类型
 *
 * @return Json::Value 对象
 * ************************************************************************
 */
template <typename T>
Json::Value to_json(const T &structure) {
    static_assert(
        std::is_aggregate_v<T>,
        "serialization::to_json: T must be a C++ aggregate (POD) type. "
        "Ensure no private members, user-defined constructors, or virtual functions.");
    Json::Value json;

    constexpr auto names = pfr::names_as_array<T>();
    auto values = pfr::structure_to_tuple(structure);

    std::apply(
        [&json, &names](const auto &...struct_fields) {
            size_t i = 0;
            // 对每个字段，使用 to_json_value 递归处理
            (detail::to_json_value(json[std::string(names[i++])],
                                   struct_fields),
             ...);
        },
        values);

    return json;
}

/**
 * ************************************************************************
 * @brief Json::Value 转 String
 *
 * @param[in] j  Json::Value 对象
 * @param[in] dent  缩进,默认 0 缩进紧凑格式
 *
 * @return
 * ************************************************************************
 */
static inline std::string dump(const Json::Value &j, int dent = 0) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = std::string(dent, ' '); // 设置缩进为空格
    builder["enableYAMLCompatibility"] = false;
    builder["precision"] = 2; // 双精度浮点数精度

    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream os;
    writer->write(j, &os);
    return os.str();
}
} // namespace module