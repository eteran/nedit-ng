
#ifndef YAML_H_
#define YAML_H_

#include <QString>

#include <yaml-cpp/yaml.h>

namespace YAML {

template <>
struct convert<QString> {
	static Node encode(const QString &rhs) {
		return Node(rhs.toStdString());
	}

	static bool decode(const Node &node, QString &rhs) {
		if (!node.IsScalar()) {
			return false;
		}

		rhs = QString::fromStdString(node.as<std::string>());
		return true;
	}
};

template <>
struct convert<QStringList> {
	static Node encode(const QStringList &rhs) {
		Node node(NodeType::Sequence);
		for (const auto &str : rhs) {
			node.push_back(str.toStdString());
		}
		return node;
	}

	static bool decode(const Node &node, QStringList &rhs) {
		if (!node.IsSequence()) {
			return false;
		}

		rhs.clear();
		for (const auto &item : node) {
			rhs.append(QString::fromStdString(item.as<std::string>()));
		}
		return true;
	}
};

}

#endif
