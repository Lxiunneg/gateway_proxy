// my_json.h
#pragma once

/**
 * ************************************************************************
 *
 * @file my_json.h 要求 C++20 使用
 * @author 刘杰
 * @brief 序列化 to_json:: 基于 Boost.pfr、Jsoncpp 与 C++17折叠表达式,使 聚合POD 类型获得 '定义即序列化' 的能力，无需编写序列化接口，只出不进
 *  json 与 STL 的对应关系
 *  json_array -> std::vector
 * @brief 反序列化 from_json:: 提取json元素
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
#include <format> //

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
} // namespace module::to_json

/**
 * ************************************************************************
 * @brief from_json 只支持到 字段 级
 *
 * TODO: 2026年度 开发计划，递归嵌套级反序列到 组合式 POD
 * ************************************************************************
 */
namespace module::from_json {
template <typename T>
concept FROM_JSON_TYPE =
    std::is_same_v<T, std::string>
    || std::is_same_v<T, int>
    || std::is_same_v<T, float>
    || std::is_same_v<T, bool>;

namespace detail {
template <FROM_JSON_TYPE T>
static inline std::string get_expect_type() {
    if constexpr (std::is_same_v<T, std::string>) {
        return "String";
    } else if constexpr (std::is_same_v<T, int>) {
        return "Integer";
    } else if constexpr (std::is_same_v<T, float>) {
        return "Real";
    } else if constexpr (std::is_same_v<T, bool>) {
        return "Boolean";
    }
}
} // namespace detail

/**
 * ************************************************************************
 * @brief 安全获取 Json 的值
 *
 * @param[in] root  Json::Value
 * @param[in] key  键
 *
 * @return 值
 * @exception 键不存在或类型错误,throw std::invalid_argument
 * ************************************************************************
 */
template <FROM_JSON_TYPE T>
static inline T get_value(const Json::Value &root, std::string_view key) {
    // 成员校验
    if (!root.isMember(key.data())) {
        throw std::invalid_argument(std::format("key[{}] 不是 json 的成员!", key));
    }

    // 类型校验()
    if (!root[key.data()].is<T>()) {
        throw std::invalid_argument(std::format("key[{}] 不是期望的类型 {}!", key, detail::get_expect_type<T>()));
    }

    return root[key.data()].as<T>(); // Jsoncpp 最新版支持 as<T>()
}

/**
 * ************************************************************************
 * @brief 安全获取 Json 的值(默认值)
 *
 * @param[in] root  Json::Value
 * @param[in] key  键
 *
 * @return
 * ************************************************************************
 */
template <FROM_JSON_TYPE T>
static inline T get_value_or(const Json::Value &root, std::string_view key, T data) {
    try {
        return get_value<T>(root, key);
    } catch (const std::invalid_argument&) {
        return data;
    }
}

} // namespace module::from_json
