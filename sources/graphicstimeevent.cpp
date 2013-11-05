/*
Copyright: LaBRI / SCRIME

Authors : Jaime Chao, Clément Bossut (2013-2014)

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/

#include "graphicstimeevent.hpp"

GraphicsTimeEvent::GraphicsTimeEvent(const QPointF &position, QGraphicsItem *parent, QGraphicsScene *scene)
  : QGraphicsObject(parent), _scene(scene), _penWidth(1), _circleRadii(10), _height(100)
{
  setFlags(QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemSendsGeometryChanges);

 // qDebug("visible %d", isVisible());

  setPos(position);
  _scene->clearSelection();
  _scene->addItem(this);
  setSelected(true);
}

void GraphicsTimeEvent::setDate(QDate date)
{
  _date = date;
}

QRectF GraphicsTimeEvent::boundingRect() const
{
  return QRectF(-_circleRadii - _penWidth/2, -_circleRadii - _penWidth / 2,
                2*_circleRadii + _penWidth, 2*_circleRadii + _height + _penWidth);
}

void GraphicsTimeEvent::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

  QPen pen(Qt::SolidPattern, _penWidth);
  painter->setPen(pen);
  painter->drawLine(0,_circleRadii, 0, _circleRadii +_height);
  painter->drawEllipse(QPointF(0,0), _circleRadii, _circleRadii);
}

QPainterPath GraphicsTimeEvent::shape() const
{
  QPainterPath path;
  path.addEllipse(QPointF(0,0), _circleRadii, _circleRadii);
  path.addRect(0,_circleRadii, _penWidth, _height); /// We can select the object 1 pixel surrounding the line
  return path;
}

void GraphicsTimeEvent::keyPressEvent(QKeyEvent *event)
{
  QGraphicsObject::keyPressEvent(event);
}

void GraphicsTimeEvent::keyReleaseEvent(QKeyEvent *event)
{
}

void GraphicsTimeEvent::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsObject::mousePressEvent(event);
}

void GraphicsTimeEvent::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsObject::mouseMoveEvent(event);
}

void GraphicsTimeEvent::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
  QGraphicsObject::mouseReleaseEvent(event);
}

void GraphicsTimeEvent::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
}

QVariant GraphicsTimeEvent::itemChange(GraphicsItemChange change, const QVariant &value)
{
  return QGraphicsObject::itemChange(change, value);
}
