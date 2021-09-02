#include "SnakeController.hpp"

#include <algorithm>
#include <sstream>

#include "EventT.hpp"
#include "IPort.hpp"

namespace Snake
{
ConfigurationError::ConfigurationError()
    : std::logic_error("Bad configuration of Snake::Controller.")
{}

UnexpectedEventException::UnexpectedEventException()
    : std::runtime_error("Unexpected event received!")
{}

Controller::Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config)
    : m_displayPort(p_displayPort),
      m_foodPort(p_foodPort),
      m_scorePort(p_scorePort)
{
    std::istringstream istr(p_config);
    char w, f, s, d;

    int width, height, length;
    int foodX, foodY;
    istr >> w >> width >> height >> f >> foodX >> foodY >> s;

    if (w == 'W' and f == 'F' and s == 'S') {
        m_mapDimension = std::make_pair(width, height);
        m_foodPosition.x = foodX;
        m_foodPosition.y = foodY;

        istr >> d;
        switch (d) {
            case 'U':
                m_currentDirection = Direction_UP;
                break;
            case 'D':
                m_currentDirection = Direction_DOWN;
                break;
            case 'L':
                m_currentDirection = Direction_LEFT;
                break;
            case 'R':
                m_currentDirection = Direction_RIGHT;
                break;
            default:
                throw ConfigurationError();
        }
        istr >> length;

        while (length) {
            Segment seg;
            istr >> seg.x >> seg.y;
            seg.ttl = length--;

            m_segments.push_back(seg);
        }
    } else {
        throw ConfigurationError();
    }
}

void Controller::receive(std::unique_ptr<Event> e)
{
    tryHandleTheTimerEvent(std::move(e));
}

bool Controller::checkCollisionOfCordWithSnake(const Coordinates& cord)
{
    for (auto segment : m_segments) {
        Coordinates cordSegment{ segment.x, segment.y };
        if (cordSegment == cord) {
            return true;
        }
    }
    return false;
}

bool Controller::checkCollisionOfNewHeadWithFood(const Coordinates& cordNewHead)
{
    if (m_foodPosition == cordNewHead) {
        m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
        return true;
    }
    return false;
}

bool Controller::checkCollisionOfNewHeadWithWalls(const Coordinates& cordNewHead)
{
    if (cordNewHead.x < 0 or cordNewHead.y < 0 or
        cordNewHead.x >= m_mapDimension.first or
        cordNewHead.y >= m_mapDimension.second) {
            return true;
    }
    return false;    
}

bool Controller::checkCollisions(const Coordinates& cordNewHead)
{
    bool lost = checkCollisionOfCordWithSnake(cordNewHead);
    if(not lost) { 
        if (not checkCollisionOfNewHeadWithFood(cordNewHead)) {
            if (checkCollisionOfNewHeadWithWalls(cordNewHead)) {
                lost = true;
            } else {
                clearCellsWithSegmentsWithLostTTL();
            }
        }
    }
    return lost;
}

Controller::Segment Controller::createNewHead()
{
    Segment const& currentHead = m_segments.front();

    Segment newHead;
    newHead.x = currentHead.x + ((m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.y = currentHead.y + (not (m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.ttl = currentHead.ttl;

    return newHead;
}

void Controller::clearCellsWithSegmentsWithLostTTL()
{
    for (auto &segment : m_segments) {
        if (not --segment.ttl) {
            DisplayInd l_evt;
            l_evt.x = segment.x;
            l_evt.y = segment.y;
            l_evt.value = Cell_FREE;

            m_displayPort.send(std::make_unique<EventT<DisplayInd>>(l_evt));
        }
    }
}

void Controller::addNewHead(const Segment& newHead)
{
    m_segments.push_front(newHead);
    DisplayInd placeNewHead;
    placeNewHead.x = newHead.x;
    placeNewHead.y = newHead.y;
    placeNewHead.value = Cell_SNAKE;

    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewHead));
}

void Controller::removeUnnecessarySegments()
{
    m_segments.erase(
        std::remove_if(
            m_segments.begin(),
            m_segments.end(),
            [](auto const& segment){ return not (segment.ttl > 0); }),
        m_segments.end());
}

void Controller::updateSnake()
{
    Segment newHead = createNewHead();

    Coordinates cordNewHead{ newHead.x, newHead.y };
    if (not checkCollisions(cordNewHead)) {
        addNewHead(newHead);
        removeUnnecessarySegments();
    } else {
        m_scorePort.send(std::make_unique<EventT<LooseInd>>());
    }
}

void Controller::updateDirection(const Direction& direction)
{
    if ((m_currentDirection & 0b01) != (direction & 0b01)) {
        m_currentDirection = direction;
    }
}

void Controller::updateReceivedFood(const FoodInd& receivedFood)
{
    Coordinates cordReceivedFood{ receivedFood.x, receivedFood.y };
    if (checkCollisionOfCordWithSnake(cordReceivedFood))
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    else
        updateFood(receivedFood);

    m_foodPosition = cordReceivedFood;
}

void Controller::updateRequestedFood(const FoodResp& requestedFood)
{
    Coordinates cordRequestedFood{ requestedFood.x, requestedFood.y };
    if (checkCollisionOfCordWithSnake(cordRequestedFood)) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        DisplayInd placeNewFood;
        placeNewFood.x = requestedFood.x;
        placeNewFood.y = requestedFood.y;
        placeNewFood.value = Cell_FOOD;
        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
    }

    m_foodPosition = cordRequestedFood;
}

void Controller::updateFood(const FoodInd& receivedFood)
{
    DisplayInd clearOldFood;
    clearOldFood.x = m_foodPosition.x;
    clearOldFood.y = m_foodPosition.y;
    clearOldFood.value = Cell_FREE;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(clearOldFood));

    DisplayInd placeNewFood;
    placeNewFood.x = receivedFood.x;
    placeNewFood.y = receivedFood.y;
    placeNewFood.value = Cell_FOOD;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
}

void Controller::tryHandleTheTimerEvent(std::unique_ptr<Event> e)
{
    try {
        auto const& timerEvent = *dynamic_cast<EventT<TimeoutInd> const&>(*e);
        updateSnake();
    } catch (std::bad_cast&) {
        tryHandleTheDirectionEvent(std::move(e));
    }
}

void Controller::tryHandleTheDirectionEvent(std::unique_ptr<Event> e)
{
    try {
        auto direction = dynamic_cast<EventT<DirectionInd> const&>(*e)->direction;
        updateDirection(direction);
    } catch (std::bad_cast&) {
        tryHandleTheReceivedFoodEvent(std::move(e));
    }
}

void Controller::tryHandleTheReceivedFoodEvent(std::unique_ptr<Event> e)
{
    try {
        auto receivedFood = *dynamic_cast<EventT<FoodInd> const&>(*e);
        updateReceivedFood(receivedFood);
    } catch (std::bad_cast&) {
        tryHandleTheRequestedFoodEvent(std::move(e));
    }
}

void Controller::tryHandleTheRequestedFoodEvent(std::unique_ptr<Event> e)
{
    try {
        auto requestedFood = *dynamic_cast<EventT<FoodResp> const&>(*e);
        updateRequestedFood(requestedFood);
    } catch (std::bad_cast&) {
        throw UnexpectedEventException();
    }
}

} // namespace Snake
