#include "market_order_book.h"

#include "trade_engine.h"

namespace Trading {
    MarketOrderBook::MarketOrderBook(TickerId ticker_id, Logger* logger)
        : ticker_id_(ticker_id), orders_at_price_pool_(ME_MAX_PRICE_LEVELS), order_pool_(ME_MAX_ORDER_IDS), logger_(logger) {
    }

    MarketOrderBook::~MarketOrderBook() {
        logger_->log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__,
            Common::getCurrentTimeStr(&time_str_), toString(false, true));

        trade_engine_ = nullptr;
        bids_by_price_ = asks_by_price_ = nullptr;
        oid_to_order_.fill(nullptr);
    }

    auto MarketOrderBook::onMarketUpdate(const Exchange::MEMarketUpdate* market_update) noexcept -> void {
        const auto bid_updated = (bids_by_price_ && market_update->side_) == Side::BUY && market_update->price_ >= bids_by_price_->price_);
        const auto ask_updated = (asks_by_price_ && market_update->side_ == Side::SELL && market_update->price_ <= asks_by_price_->price_);

        switch (market_update->type_) {
        case Exchange::MarketUpdateType::ADD: {
            auto order = order_pool_.allocate(market_update->order_id_, market_update->side_, market_update->price_,
                market_update->qty_, market_update->priority_, nullptr, nullptr);
            addOrder(order);
        }
                                            break;
        case Exchange::MarketUpdateType::MODIFY: {
            auto order = oid_to_order_.at(market_update->order_id_);
            order->qty_ = market_update->qty_;
        }
                                               break;
        case Exchange::MarketUpdateType::CANCEL: {
            auto order = oid_to_order_.at(market_update->order_id_);
            removeOrder(order);
        }
                                               break;
        case Exchange::MarketUpdateType::TRADE: {
            trade_engine_->onTradeUpdate(market_update, this);
            return;
        }
                                              break;
        case Exchange::MarketUpdateType::CLEAR: {
            for (auto& order : oid_to_order_) {
                if (order)
                    order_pool_.deallocate(order);
            }
            oid_to_order_.fill(nullptr);

            if (bids_by_price_) {
                for (auto bid = bids_by_price_->next_entry_; bid != bids_by_price_; bid = bid->next_entry_)
                    orders_at_price_pool_.deallocate(bid);
                orders_at_price_pool_.deallocate(bids_by_price_);
            }

            if (asks_by_price_) {
                for (auto ask = asks_by_price_->next_entry_; ask != asks_by_price_; ask = ask->next_entry_)
                    orders_at_price_pool_.deallocate(ask);
                orders_at_price_pool_.deallocate(asks_by_price_);
            }

            bids_by_price_ = asks_by_price_ = nullptr;
        }
                                              break;
        case Exchange::MarketUpdateType::INVALID:
        case Exchange::MarketUpdateType::SNAPSHOT_START:
        case Exchange::MarketUpdateType::SNAPSHOT_END:
            break;
        }

        updateBBO(bid_updated, ask_updated);

        //Notifies trading engine
        logger_->log("%:% %() % % %", __FILE__, __LINE__, __FUNCTION__,
            Common::getCurrentTimeStr(&time_str_), market_update->toString(), bbo_.toString());

        trade_engine_->onOrderBookUpdate(market_update->ticker_id_, market_update->price_, market_update->side_, this);
    }
}