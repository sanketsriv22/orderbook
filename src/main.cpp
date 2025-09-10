#include <iostream>
#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <limits>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <tuple>
#include <format>

enum class OrderType
{
    GoodTillCancel,
    FillAndKill
};

enum class Side
{
    Buy,
    Sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
// vector of orderId's?

struct LevelInfo
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos
{
public:
    OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) :
        bids_ { bids },
        asks_ { asks }
    {}

    const LevelInfos& GetBids() const { return bids_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};


class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) : 
        orderType_{ orderType },
        orderId_{ orderId },
        side_{ side },
        price_{ price },
        initialQuantity_{ quantity },
        remainingQuantity_{ quantity }
    {}

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }    
    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity.", GetOrderId()));
        
        remainingQuantity_ -= quantity;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;

};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>; // list is non-contiguous

class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity) :
        orderId_{ orderId },
        price_{ price },
        side_{ side },
        quantity_{ quantity }
    {}
    OrderId GetOrderId() const { return orderId_; }
    Price GetPrice() const { return price_; }
    Side GetSide() const { return side_; }
    Quantity GetQuantity() const { return quantity_; }

    // public API to converting a new order with OrderModify into an entirely separate order
    OrderPointer ToOrderPointer(OrderType type) const
    {
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }
private:
    OrderId orderId_;
    Price price_;
    Side side_;
    Quantity quantity_;
};

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade) :
        bidTrade_{ bidTrade },
        askTrade_{ askTrade }
    {}

    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

class Orderbook
{
private:
    // using map for bids and asks storage
    struct OrderEntry
    {
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;
    };

    // ordered container, std::greater is descending (highest bid at top)
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    // ordered container, std::less is ascending (lowest ask at top)
    std::map<Price, OrderPointers, std::less<Price>> asks_;

    // hashmap, key OrderId, value orderEntry
    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(Side side, Price price) const
    {
        if (side == Side::Buy)
        {
            if (asks_.empty())
                return false;

            const auto& [bestAsk, _] = *asks_.begin();
            // if price is to buy is greater than lowest ask, valid sale
            return price >= bestAsk;
        }
        else
        {
            if (bids_.empty())
                return false;
            
            const auto& [bestBid, _] = *bids_.begin();
            // if price is to sell is less than bid, valid sale
            return price <= bestBid; 
        }
    }
    Trades MatchOrders()
    {
        Trades trades;
        trades.reserve(orders_.size());

        while (true)
        {
            if (bids_.empty() || asks_.empty())
                break;

            // begin() returns an iterator to the first element of map, so we dereference it to get the pair
            auto& [bidPrice, bids] = *bids_.begin(); // all bids have the same price here
            auto& [askPrice, asks] = *asks_.begin(); // all asks have the same price here

            if (bidPrice < askPrice)
                break; // no matches

            while(bids.size() && asks.size())
            {
                auto& bid = bids.front(); // first bid in queue at this price
                auto& ask = asks.front(); // first ask in queue at this price

                // quantity sold here is min of either remainingquantity of bid or ask
                Quantity quantity = std::min(bid->GetRemainingQuantity(),ask->GetRemainingQuantity());

                // now that we have matched quantity, we can fill the order
                bid->Fill(quantity);
                ask->Fill(quantity);
               
                if (bid->IsFilled()) // is remaining quantity 0
                {
                    bids.pop_front();                 // remove from bids queue (OrderPointers std::vector)
                    orders_.erase(bid->GetOrderId()); // bid no longer valid
                }

                if (ask->IsFilled())
                {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }

                if (bids.empty()) // no more bids with this price left
                    bids_.erase(bidPrice);
                if (asks.empty()) // no more bids with this price left
                    asks_.erase(askPrice);

                trades.push_back(Trade{
                    TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
                    TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity }
                });

             }
        }
        
        if (!bids_.empty())
        {
            auto& [_, bids] = *bids_.begin();
            auto& order = bids.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
        if (!asks_.empty())
        {
            auto& [_, asks] = *asks_.begin();
            auto& order = asks.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
    }
public:

    Trades AddOrder(OrderPointer order)
    {
        if (orders_.contains(order->GetOrderId()))
            return {};
        if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
            return {};

        OrderPointers::iterator iterator;

        if (order->GetSide() == Side::Buy)
        {
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            // iterator = orders.insert(orders.end(), order); returns an iterator too
            iterator = std::next(orders.begin(), orders.size() - 1);
        }
        else
        {
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        }

        orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });
        return MatchOrders();
    }

    void CancelOrder(OrderId orderId)
    {
        if (!orders_.contains(orderId))
            return;

        const auto& [order, orderIterator] = orders_.at(orderId);
        // remove from order hashmap
        orders_.erase(orderId);

        // also remove from ask map with iterator as key O(1)
        if (order->GetSide() == Side::Sell)
        {
            auto price = order->GetPrice();
            auto& orders = asks_.at(price);
            orders.erase(orderIterator);
            // check if there are still any sell orders at this price
            if (orders.empty())
                asks_.erase(price);
        } else
        {
            auto price = order->GetPrice();
            auto& orders = bids_.at(price);
            orders.erase(orderIterator);
            // check to see if any buy orders at this price 
            if (orders.empty())
            {
                bids_.erase(price);
            }
        }
    }

    Trades MatchOrder(OrderModify order)
    {
        if (!orders_.contains(order.GetOrderId()))
            return {};


    }
};

int main()
{
    std::cout << "Starting repo on an orderbook for trades." << std::endl;
    return 0;
}