#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorer/NodePath.hpp"

namespace DeviceExplorer
{
    namespace Command
    {

        class Remove : public iscore::SerializableCommand
        {
            ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(Remove, "DeviceExplorerControl")

                Remove(ObjectPath&& device_tree, Path nodePath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            protected:
                ObjectPath m_deviceTree;
                Path m_parentPath;
                Path m_nodePath;
                AddressSettings m_addressSettings;
                int m_nodeIndex{};

                //TODO : QByteArray m_serializedNode;
                Node* m_node{};
        };
    }
}
