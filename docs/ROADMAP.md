# Roadmap

## Phase 1: Core Data Structures
- [ ] `Order` struct with fixed-point pricing
- [ ] `PriceLevel` with intrusive linked list
- [ ] `OrderBook` with bid/ask sides
- [ ] Unit tests for all core types

## Phase 2: Matching Engine
- [ ] Price-time priority matching (Limit orders)
- [ ] Market orders
- [ ] IOC (Immediate or Cancel)
- [ ] FOK (Fill or Kill)
- [ ] Execution event generation

## Phase 3: Benchmarking
- [ ] Synthetic order stream generator
- [ ] Throughput benchmark (orders/sec)
- [ ] Latency benchmark (p50, p95, p99)
- [ ] Baseline results documented

## Phase 4: Performance Optimization
- [ ] Arena allocator for Order objects
- [ ] Optimized price level container
- [ ] Hot/cold struct splitting
- [ ] Lock-free SPSC ring buffer

## Phase 5: Protocol Layer
- [ ] FIX 4.2 message parser
- [ ] Binary protocol design
- [ ] TCP gateway server

## Phase 6: Market Data & Observability
- [ ] L2 order book snapshots
- [ ] Structured event logging
- [ ] Replay capability

## Phase 7: Polish
- [ ] Architecture diagrams
- [ ] Full API documentation
- [ ] CI benchmark regression checks
