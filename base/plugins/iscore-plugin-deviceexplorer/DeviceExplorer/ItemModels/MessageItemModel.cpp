#include "MessageItemModel.hpp"
#include "NodeDisplayMethods.hpp"
#include "Commands/AddMessagesToModel.hpp"
#include <State/MessageListSerialization.hpp>
#include <iscore/document/DocumentInterface.hpp>

using namespace iscore;
MessageItemModel::MessageItemModel(QObject* parent):
    NodeBasedItemModel{parent},
    m_rootNode{}
{
    this->setObjectName("MessageItemModel");
}

MessageItemModel &MessageItemModel::operator=(const MessageItemModel & other)
{
    beginResetModel();
    m_rootNode = other.m_rootNode;
    endResetModel();
    return *this;
}

MessageItemModel &MessageItemModel::operator=(const iscore::Node & n)
{
    beginResetModel();
    m_rootNode = n;
    endResetModel();
    return *this;
}

MessageItemModel &MessageItemModel::operator=(iscore::Node && n)
{
    beginResetModel();
    m_rootNode = std::move(n);
    endResetModel();
    return *this;
}

void MessageItemModel::setCommandStack(ptr<CommandStack> stk)
{
    m_stack = stk;
}

int MessageItemModel::columnCount(const QModelIndex &parent) const
{
    return (int)Column::Count;
}

QVariant MessageItemModel::data(const QModelIndex &index, int role) const
{
    const int col = index.column();

    if(col < 0 || col >= (int)Column::Count)
        return {};

    auto node = nodeFromModelIndex(index);

    if(! node)
        return {};

    switch((Column)col)
    {
        case Column::Name:
        {
            return nameColumnData(*node, role);
            break;
        }
        case Column::Value:
        {
            return valueColumnData(*node, role);
            break;
        }
        default:
            break;
    }

    return {};
}

QVariant MessageItemModel::headerData(
        int section,
        Qt::Orientation orientation,
        int role) const
{
    static const QMap<Column, QString> HEADERS{
        {Column::Name, QObject::tr("Address")},
        {Column::Value, QObject::tr("Value")}
    };
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if(section >= 0 && section < (int)Column::Count)
        {
            return HEADERS[(Column)section];
        }
    }

    return QVariant();
}

bool MessageItemModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    return false;
}

QStringList MessageItemModel::mimeTypes() const
{
    return {iscore::mime::state()};
}

QMimeData *MessageItemModel::mimeData(const QModelIndexList &indexes) const
{
    return nullptr;
}

bool MessageItemModel::canDropMimeData(
        const QMimeData *data,
        Qt::DropAction action,
        int row,
        int column,
        const QModelIndex &parent) const
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        return false;
    }

    // TODO extended to accept mime::device, mime::address, mime::node?
    if(! data || (! data->hasFormat(iscore::mime::messagelist())))
    {
        return false;
    }

    return true;
}

bool MessageItemModel::dropMimeData(
        const QMimeData *data,
        Qt::DropAction action,
        int row,
        int column,
        const QModelIndex &parent)
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        return false;
    }

    // TODO extended to accept mime::device, mime::address, mime::node?
    if(! data || (! data->hasFormat(iscore::mime::messagelist())))
    {
        return false;
    }

    iscore::MessageList ml;
    fromJsonArray(
                QJsonDocument::fromJson(data->data(iscore::mime::messagelist())).array(),
                ml);

    auto cmd = new AddMessagesToModel{
               iscore::IDocument::path(*this),
               ml};

    CommandDispatcher<> disp(*m_stack);
    disp.submitCommand(cmd);

    return true;
}

Qt::DropActions MessageItemModel::supportedDropActions() const
{
    return Qt::IgnoreAction;
}

Qt::DropActions MessageItemModel::supportedDragActions() const
{
    return Qt::IgnoreAction;
}


Qt::ItemFlags MessageItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled;
    if(index.isValid())
    {
        f |= Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
        if(index.column() == (int) Column::Name)
            f |= Qt::ItemIsDropEnabled;

        Node* n = nodeFromModelIndex(index);
        if(n->isEditable())
            f |= Qt::ItemIsEditable;
    }
    else
    {
        f |= Qt::ItemIsDropEnabled;
    }
    return f;
}


#include "Plugin/Commands/EditData.hpp"
bool MessageItemModel::setData(
        const QModelIndex& index,
        const QVariant& value,
        int role)
{
    if(! index.isValid())
        return false;

    auto n = nodeFromModelIndex(index);
    // TODO assert that this doesn't return nullptr.
    if(! n)
        return false;

    if(!n->is<AddressSettings>())
        return false;

    const auto& addr = n->get<iscore::AddressSettings>();
    if(addr.ioType != IOType::InOut && addr.ioType != IOType::Out)
        return false;

    auto col = Column(index.column());

    if(role == Qt::EditRole)
    {
        if(col == Column::Value)
        {
            // In this case we don't make a command, but we directly push the
            // new value.
            QVariant copy = value;
            auto res = copy.convert(addr.value.val.type());
            if(res)
            {
                auto cmd = new EditValue(iscore::IDocument::path(*this),
                                         iscore::NodePath(*n),
                                         copy);

                CommandDispatcher<> disp(*m_stack);
                disp.submitCommand(cmd);
                return true;
            }

            return false;
        }
    }

    return false;
}

void MessageItemModel::editData(
        const NodePath& path,
        const QVariant& val)
{
    Node* node = path.toNode(&rootNode());
    ISCORE_ASSERT(node && node->parent());

    QModelIndex index = createIndex(
                            node->parent()->indexOfChild(node),
                            (int)Column::Value,
                            node->parent());

    QModelIndex changedTopLeft = index;
    QModelIndex changedBottomRight = index;

    if(!node->is<AddressSettings>())
        return;

    node->get<iscore::AddressSettings>().value.val = val;

    emit dataChanged(changedTopLeft, changedBottomRight);
}
