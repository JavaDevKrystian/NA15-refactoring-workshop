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
    initializeConfiguration(p_config);
}

void Controller::receive(std::unique_ptr<Event> e)
{
    tryHandleTheTimerEvent(std::move(e));
}

void Controller::initializeConfiguration(std::string const& p_config)
{
    std::istringstream istr(p_config);
    char w, f, s, d;
    int width, height;
    int foodX, foodY;
    istr >> w >> width >> height >> f >> foodX >> foodY >> s >> d;

    if (w == 'W' and f == 'F' and s == 'S') {
        initializeMapDimension(width, height);
        initializeFoodPosition(foodX, foodY);
        setCurrentDirection(d);
        createSegments(std::move(istr));
    } else {
        throw ConfigurationError();
    }
}

void Controller::setCurrentDirection(char d)
{
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
}

void Controller::createSegments(std::istringstream istr)
{
    int length;
    istr >> length;
    while (length) {
        Segment seg;
        istr >> seg.cord.x >> seg.cord.y;
        seg.ttl = length--;
        m_segments.push_back(seg);
    }
}

void Controller::initializeMapDimension(int width, int height)
{
    m_mapDimension = std::make_pair(width, height);
}

void Controller::initializeFoodPosition(int x, int y)
{
    m_foodPosition.x = x;
    m_foodPosition.y = y;
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

void Controller::updateSnake()
{
    Segment newHead = createNewHead();
    if (not checkCollisions(newHead.cord)) {
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
    if (checkCollisionOfCordWithSnake(cordReceivedFood)) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        updateFood(cordReceivedFood);
    }
    m_foodPosition = cordReceivedFood;
}

void Controller::updateRequestedFood(const FoodResp& requestedFood)
{
    Coordinates cordRequestedFood{ requestedFood.x, requestedFood.y };
    if (checkCollisionOfCordWithSnake(cordRequestedFood)) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        sendDisplayIndEvent(cordRequestedFood, Cell::Cell_FOOD);
    }
    m_foodPosition = cordRequestedFood;
}

void Controller::updateFood(const Coordinates& cordReceivedFood)
{
    sendDisplayIndEvent(m_foodPosition, Cell::Cell_FREE);
    sendDisplayIndEvent(cordReceivedFood, Cell::Cell_FOOD);
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

bool Controller::checkCollisionOfCordWithSnake(const Coordinates& cord)
{
    for (auto segment : m_segments) {
        if (segment.cord == cord) {
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

Controller::Segment Controller::createNewHead()
{
    Segment const& currentHead = m_segments.front();

    Segment newHead;
    newHead.cord.x = currentHead.cord.x + ((m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.cord.y = currentHead.cord.y + (not (m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.ttl = currentHead.ttl;

    return newHead;
}

void Controller::sendDisplayIndEvent(const Coordinates& cord, const Cell& value)
{
    DisplayInd event;
    event.x = cord.x;
    event.y = cord.y;
    event.value = value;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(event));
}

void Controller::clearCellsWithSegmentsWithLostTTL()
{
    for (auto &segment : m_segments) {
        if (not --segment.ttl) {
            sendDisplayIndEvent(segment.cord, Cell::Cell_FREE);
        }
    }
}

void Controller::addNewHead(const Segment& newHead)
{
    m_segments.push_front(newHead);
    sendDisplayIndEvent(newHead.cord, Cell::Cell_SNAKE);
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

} // namespace Snake
