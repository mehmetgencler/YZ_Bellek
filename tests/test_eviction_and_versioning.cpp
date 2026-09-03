#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "memory_graph.hpp"
#include "declarative.hpp"
#include "procedural.hpp"
#include "uuid.hpp"
#include <filesystem>
#include <thread>
#include <vector>

using namespace om;

TEST_CASE("Eviction + Versioning", "[hot][eviction][cold]") {
    std::filesystem::remove_all("./test_cold");
    MemoryGraph graph("./test_cold");
    // small hot cache to trigger eviction
    graph.hot_ = HotCache(2);

    // Create three facts and insert them via transaction
    auto fact1 = std::make_shared<Fact>(generate_uuid(), "userA");
    fact1->entity = "e1"; fact1->predicate = "p1";
    auto fact2 = std::make_shared<Fact>(generate_uuid(), "userA");
    fact2->entity = "e2"; fact2->predicate = "p2";
    auto fact3 = std::make_shared<Fact>(generate_uuid(), "userA");
    fact3->entity = "e3"; fact3->predicate = "p3";

    {
        Transaction tx(graph);
        tx.stage_create(fact1);
        tx.stage_create(fact2);
        tx.stage_create(fact3);
        bool ok = tx.commit();
        REQUIRE(ok);
    }

    // Hot cache capacity 2 ensures at least 1 eviction occurred that saved to cold storage
    // Check that files exist with the assigned version inside
    // All three should be in graph.objects_ with the same assigned version
    uint64_t version = 0;
    {
        std::shared_lock<std::shared_mutex> lock(graph.mutex_);
        REQUIRE(graph.objects_.size() == 3);
        for (auto &p : graph.objects_) {
            version = p.second.version;
            break;
        }
    }
    // Verify cold storage contains at least one file with version field
    bool found_version_on_disk = false;
    for (auto &p : graph.objects_) {
        auto path = std::filesystem::path("./test_cold") / "data" / (p.first + ".json");
        if (std::filesystem::exists(path)) {
            std::ifstream in(path);
            nlohmann::json j; in >> j;
            if (j.value("version", 0u) == version) found_version_on_disk = true;
        }
    }
    REQUIRE(found_version_on_disk);
}

TEST_CASE("Lazy Load", "[cold][lazy]") {
    std::filesystem::remove_all("./test_cold2");
    MemoryGraph graph("./test_cold2");
    graph.hot_ = HotCache(1);

    auto fact1 = std::make_shared<Fact>(generate_uuid(), "userB");
    fact1->entity = "le1"; fact1->predicate = "lp1";

    {
        Transaction tx(graph);
        tx.stage_create(fact1);
        REQUIRE(tx.commit());
    }

    // Force eviction by inserting fake objects to hot cache
    auto fact2 = std::make_shared<Fact>(generate_uuid(), "userB");
    fact2->entity = "x"; fact2->predicate = "y";
    {
        Transaction tx(graph);
        tx.stage_create(fact2);
        REQUIRE(tx.commit());
    }

    // Now evict and clear hot cache
    graph.hot_.clear();

    // get_object should lazy-load from cold and re-insert
    auto maybe = graph.get_object(fact1->id());
    REQUIRE(maybe.has_value());
    REQUIRE(maybe->obj->type() == ObjectType::Fact);
    REQUIRE(maybe->version >= 1);
}

TEST_CASE("MVCC Snapshot conflict", "[mvcc]") {
    std::filesystem::remove_all("./test_cold3");
    MemoryGraph graph("./test_cold3");

    auto fact = std::make_shared<Fact>(generate_uuid(), "userC");
    fact->entity = "z"; fact->predicate = "w";

    {
        Transaction tx(graph);
        tx.stage_create(fact);
        REQUIRE(tx.commit());
    }

    // T1 reads
    Transaction t1(graph);
    auto read_t1 = t1.read(fact->id());
    REQUIRE(read_t1);

    // T2 updates the fact (replace)
    Transaction t2(graph);
    auto updated = std::make_shared<Fact>(generate_uuid(), "userC");
    updated->entity = "z"; updated->predicate = "w"; // new id to simulate update semantics
    t2.stage_create(updated);
    REQUIRE(t2.commit());

    // Now t1 attempts commit (should fail due to version mismatch)
    auto new_obj_for_t1 = std::make_shared<Fact>(generate_uuid(), "userC");
    new_obj_for_t1->entity = "z"; new_obj_for_t1->predicate = "w";
    t1.stage_create(new_obj_for_t1);
    REQUIRE(!t1.commit());
}

TEST_CASE("Lock ordering stress", "[locks][stress]") {
    std::filesystem::remove_all("./test_cold4");
    MemoryGraph graph("./test_cold4");
    graph.hot_ = HotCache(50);

    // Start several threads performing random get_object and commit
    std::vector<std::thread> threads;
    const int n = 100;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&graph, n](){
            for (int k = 0; k < n; ++k) {
                // create a transient fact and commit
                auto f = std::make_shared<Fact>(generate_uuid(), "stress");
                f->entity = "e"; f->predicate = "p";
                Transaction t(graph);
                t.stage_create(f);
                t.commit();
                // random get
                graph.get_object(f->id());
            }
        });
    }
    for (auto &t : threads) t.join();
    SUCCEED("Completed concurrent operations without deadlock in test environment");
}
