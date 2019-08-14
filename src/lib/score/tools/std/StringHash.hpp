#pragma once
#include <QString>
#include <QHashFunctions>
#include <functional>

#include <ossia-config.hpp>

// TODO merge String.hpp here
namespace std
{
template <>
struct hash<QString>
{
  std::size_t operator()(const QString& path) const { return qHash(path); }
};
}
