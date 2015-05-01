#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class Client;
class Group;
class AddClientToGroup : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("AddClientToGroup", "AddClientToGroup")
    public:
        ISCORE_COMMAND_DEFAULT_CTOR(AddClientToGroup, "NetworkControl")
        AddClientToGroup(ObjectPath&& groupMgrPath,
                         id_type<Client> client,
                         id_type<Group> group);

        virtual void undo() override;
        virtual void redo() override;

        virtual void serializeImpl(QDataStream & s) const override;
        virtual void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_path;
        id_type<Client> m_client;
        id_type<Group> m_group;
};
