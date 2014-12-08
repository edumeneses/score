#include "ScenarioProcessPresenter.hpp"
#include "ScenarioProcessSharedModel.hpp"
#include "ScenarioProcessViewModel.hpp"
#include "ScenarioProcessView.hpp"

#include "Interval/IntervalView.hpp"
#include "Interval/IntervalPresenter.hpp"
#include "Interval/IntervalModel.hpp"
#include "Interval/IntervalContent/IntervalContentView.hpp"
#include "Interval/IntervalContent/IntervalContentPresenter.hpp"

#include "Event/EventModel.hpp"
#include "Event/EventPresenter.hpp"
#include "Event/EventView.hpp"

#include "Commands/Scenario/CreateEventCommand.hpp"
#include "Commands/Scenario/CreateEventAfterEventCommand.hpp"
#include "Commands/Scenario/MoveEventCommand.hpp"
#include "Commands/Scenario/MoveIntervalCommand.hpp"

#include <QDebug>

#include <QRectF>

#include <tools/utilsCPP11.hpp>

// TODO Question :
// étirement temporel d'une boîte qui contient un scénario hiérarchique ?
// veut on étirer les choses ou les laisser à leur place ?


ScenarioProcessPresenter::ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model,
												   iscore::ProcessViewInterface* view,
												   QObject* parent):
	iscore::ProcessPresenterInterface{parent, "ScenarioProcessPresenter"},
	m_viewModel{static_cast<ScenarioProcessViewModel*>(model)},
	m_view{static_cast<ScenarioProcessView*>(view)}
{
	/////// Setup of existing data
	// For each interval & event, display' em
	for(auto interval_model : m_viewModel->model()->intervals())
	{
		on_intervalCreated_impl(interval_model);
	}

	for(auto event_model : m_viewModel->model()->events())
	{
		on_eventCreated_impl(event_model);
	}

	/////// Connections
	connect(this,	SIGNAL(submitCommand(iscore::SerializableCommand*)),
			parent, SIGNAL(submitCommand(iscore::SerializableCommand*)));

	connect(this,	SIGNAL(elementSelected(QObject*)),
			parent, SIGNAL(elementSelected(QObject*)));

	connect(m_view, &ScenarioProcessView::scenarioPressed,
			this,	&ScenarioProcessPresenter::on_scenarioPressed);
	connect(m_view, &ScenarioProcessView::scenarioReleased,
			this,	&ScenarioProcessPresenter::on_scenarioReleased);

	connect(m_viewModel, &ScenarioProcessViewModel::eventCreated,
			this,		 &ScenarioProcessPresenter::on_eventCreated);
	connect(m_viewModel, &ScenarioProcessViewModel::eventDeleted,
			this,		 &ScenarioProcessPresenter::on_eventDeleted);
	connect(m_viewModel, &ScenarioProcessViewModel::intervalCreated,
			this,		 &ScenarioProcessPresenter::on_intervalCreated);
	connect(m_viewModel, &ScenarioProcessViewModel::intervalDeleted,
			this,		 &ScenarioProcessPresenter::on_intervalDeleted);
	connect(m_viewModel, &ScenarioProcessViewModel::eventMoved,
			this,		 &ScenarioProcessPresenter::on_eventMoved);
	connect(m_viewModel, &ScenarioProcessViewModel::intervalMoved,
			this,		 &ScenarioProcessPresenter::on_intervalMoved);


}

int ScenarioProcessPresenter::id() const
{
	return m_viewModel->model()->id();
}

int ScenarioProcessPresenter::currentlySelectedEvent() const
{
	return m_currentlySelectedEvent;
}

void ScenarioProcessPresenter::on_eventCreated(int eventId)
{
	on_eventCreated_impl(m_viewModel->model()->event(eventId));
}

void ScenarioProcessPresenter::on_intervalCreated(int intervalId)
{
	on_intervalCreated_impl(m_viewModel->model()->interval(intervalId));
}

void ScenarioProcessPresenter::on_eventDeleted(int eventId)
{
	removeFromVectorWithId(m_events, eventId);
	m_view->update();
}

void ScenarioProcessPresenter::on_intervalDeleted(int intervalId)
{
	removeFromVectorWithId(m_intervals, intervalId);
	m_view->update();
}

void ScenarioProcessPresenter::on_eventMoved(int eventId)
{
	auto rect = m_view->boundingRect();

	for(auto ev : m_events) {
		if(ev->id() == eventId ) {
			ev->view()->setTopLeft({rect.x() + ev->model()->m_x,
									rect.y() + rect.height() * ev->model()->heightPercentage()});
		}
	}
	m_view->update();
}

void ScenarioProcessPresenter::on_intervalMoved(int intervalId)
{

	auto rect = m_view->boundingRect();

	for(auto inter : m_intervals) {
		if(inter->id() == intervalId ) {
			inter->view()->setTopLeft({rect.x() + inter->model()->startDate(),
									rect.y() + rect.height() * inter->model()->heightPercentage()});
		}
	}
	m_view->update();
}


void ScenarioProcessPresenter::on_scenarioPressed(QPointF point)
{
	auto cmd = new CreateEventCommand(ObjectPath::pathFromObject("BaseIntervalModel",
																m_viewModel->model()),
									 point.x(),
									 (point - m_view->boundingRect().topLeft() ).y() / m_view->boundingRect().height() );
	submitCommand(cmd);
}

void ScenarioProcessPresenter::on_scenarioReleased(QPointF point)
{
	//createIntervalAndEventFromEvent();


	//submitCommand(cmd);
}

void ScenarioProcessPresenter::setCurrentlySelectedEvent(int arg)
{
	if (m_currentlySelectedEvent != arg) {
		m_currentlySelectedEvent = arg;
		emit currentlySelectedEventChanged(arg);
	}
}

void ScenarioProcessPresenter::createIntervalAndEventFromEvent(int id, int distance, double heightPos)
{
	auto cmd = new CreateEventAfterEventCommand(ObjectPath::pathFromObject("BaseIntervalModel",
																		   m_viewModel->model()),
												id,
												distance,
												(heightPos - m_view->boundingRect().topLeft().y())/m_view->boundingRect().height());

	submitCommand(cmd);
}

void ScenarioProcessPresenter::moveEventAndInterval(int id, int distance, double heightPos)
{
	auto cmd = new MoveEventCommand(ObjectPath::pathFromObject("BaseIntervalModel",
															   m_viewModel->model()),
									id,
									distance,
									(heightPos - m_view->boundingRect().topLeft().y())/m_view->boundingRect().height());
	qDebug() << "scenar : event move";
	submitCommand(cmd);
}

void ScenarioProcessPresenter::moveIntervalOnVertical(int id, double heightPos)
{
	int endEventId{};
	for(auto inter : m_intervals) {
		if(inter->id() == id ) {
			endEventId = inter->model()->endEvent();
		}
	}

	auto cmd = new MoveIntervalCommand(ObjectPath::pathFromObject("BaseIntervalModel",
															   m_viewModel->model()),
									id,
									endEventId,
									(heightPos - m_view->boundingRect().topLeft().y())/m_view->boundingRect().height());
	qDebug() << "scenar : interval move";
	submitCommand(cmd);
}




void ScenarioProcessPresenter::on_eventCreated_impl(EventModel* event_model)
{
	qDebug() << Q_FUNC_INFO << event_model;
	auto rect = m_view->boundingRect();

	auto event_view = new EventView{m_view};
	auto event_presenter = new EventPresenter{event_model,
											  event_view,
											  this};

	event_view->setTopLeft({rect.x() + event_model->m_x,
							rect.y() + rect.height() * event_model->heightPercentage()});

	m_events.push_back(event_presenter);

	connect(event_presenter, &EventPresenter::eventSelected,
			this,			 &ScenarioProcessPresenter::setCurrentlySelectedEvent);
	connect(event_presenter, &EventPresenter::eventReleasedWithControl,
			this,			 &ScenarioProcessPresenter::createIntervalAndEventFromEvent);
	connect(event_presenter, &EventPresenter::eventReleased,
			this,			 &ScenarioProcessPresenter::moveEventAndInterval);
}

void ScenarioProcessPresenter::on_intervalCreated_impl(IntervalModel* interval_model)
{
	auto rect = m_view->boundingRect();

	auto interval_view = new IntervalView{m_view};
	auto interval_presenter = new IntervalPresenter{interval_model,
													interval_view,
													this};

	interval_view->setTopLeft({rect.x() + interval_model->m_x,
							   rect.y() + rect.height() * interval_model->heightPercentage()});

	m_intervals.push_back(interval_presenter);

	connect(interval_presenter, &IntervalPresenter::intervalReleased,
			this,			 &ScenarioProcessPresenter::moveIntervalOnVertical);

}
