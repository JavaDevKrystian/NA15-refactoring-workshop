#pragma once

#include <list>
#include <memory>
#include <stdexcept>

#include "IEventHandler.hpp"
#include "SnakeInterface.hpp"

class Event;
class IPort;

namespace Snake
{
struct ConfigurationError : std::logic_error
{
    ConfigurationError();
};

struct UnexpectedEventException : std::runtime_error
{
    UnexpectedEventException();
};

class Controller : public IEventHandler
{
public:
    Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config);

    Controller(Controller const& p_rhs) = delete;
    Controller& operator=(Controller const& p_rhs) = delete;

    void receive(std::unique_ptr<Event> e) override;

private:
    struct Coordinates
    {
        int x;
        int y;

        bool operator==(const Coordinates& cord)
        {
            return (x == cord.x && y == cord.y);
        }
    };
    struct Segment
    {
        Coordinates cord;
        int ttl;
    };

    bool checkCollisionOfCordWithSnake(const Coordinates& cord);
    bool checkCollisionOfNewHeadWithFood(const Coordinates& cordNewHead);
    bool checkCollisionOfNewHeadWithWalls(const Coordinates& cordNewHead);
    bool checkCollisions(const Coordinates& cordNewHead);

    Segment createNewHead();
    void sendDisplayIndEvent(const Coordinates& cord, const Cell& value);
    void clearCellsWithSegmentsWithLostTTL();
    void addNewHead(const Segment& newHead);
    void removeUnnecessarySegments();

    void updateSnake();
    void updateDirection(const Direction& direction);
    void updateReceivedFood(const FoodInd& receivedFood);
    void updateRequestedFood(const FoodResp& requestedFood);
    void updateFood(const FoodInd& receivedFood);

    void tryHandleTheTimerEvent(std::unique_ptr<Event> e);
    void tryHandleTheDirectionEvent(std::unique_ptr<Event> e);
    void tryHandleTheReceivedFoodEvent(std::unique_ptr<Event> e);
    void tryHandleTheRequestedFoodEvent(std::unique_ptr<Event> e);

    IPort& m_displayPort;
    IPort& m_foodPort;
    IPort& m_scorePort;

    std::pair<int, int> m_mapDimension;
    Coordinates m_foodPosition;

    Direction m_currentDirection;
    std::list<Segment> m_segments;
};

} // namespace Snake
