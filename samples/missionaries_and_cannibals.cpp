#include <gtest/gtest.h>

#include <set>

#include "../tests/engine_fixture.h"
#include "evaluate.h"
#include "extractor.h"
#include "field.h"
#include "functional.h"
#include "infix.h"
#include "operation.h"
#include "quantifier.h"

namespace missionaries_and_cannibals {

using test::EngineFixture;

using People = std::set<int>;
using PeopleChoices = std::set<People>;

struct Banks : hashable_tag_type {
  Banks(People east_ = {}, People west_ = {})
      : east(std::move(east_)), west(std::move(west_)) {}

  People east;
  People west;

  fields(east, west)
};

PeopleChoices passengerChoices(const People& people) {
  PeopleChoices result;
  for (auto&& first : people) {
    result.insert(People{first});
    for (auto&& second : people) {
      if (first < second) {
        result.insert(People{first, second});
      }
    }
  }
  return result;
}

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/MissionariesAndCannibals/MissionariesAndCannibals.tla
struct Model : IModel {
  Boolean isSafe(auto people) {
    return people $in cannibals || get_mem((people $cap cannibals), size()) <=
                                       get_mem((people $cap missionaries), size());
  }

  Boolean move(auto passengers) {
    auto eastBank = get_mem(whoIsOnBank, east);
    auto westBank = get_mem(whoIsOnBank, west);
    auto eastRemaining = eastBank $diff passengers;
    auto westRemaining = westBank $diff passengers;
    auto eastArrived = eastBank $cup passengers;
    auto westArrived = westBank $cup passengers;
    auto passengerCount = get_mem(passengers, size());

    return passengerCount >= 1 &&
           passengerCount <= 2 &&
           ((bankOfBoat == east &&
                isSafe(eastRemaining) &&
                isSafe(westArrived) &&
                bankOfBoat++ == west &&
                whoIsOnBank++ == creator<Banks>(eastRemaining, westArrived)) ||
               (bankOfBoat == west &&
                   isSafe(westRemaining) &&
                   isSafe(eastArrived) &&
                   bankOfBoat++ == east &&
                   whoIsOnBank++ == creator<Banks>(eastArrived, westRemaining)));
  }

  Boolean typeOk() {
    auto eastBank = get_mem(whoIsOnBank, east);
    auto westBank = get_mem(whoIsOnBank, west);
    return (bankOfBoat $in riverBanks) &&
           isSafe(eastBank) &&
           isSafe(westBank) &&
           (eastBank $cap westBank) == People{} &&
           (eastBank $cup westBank) == allPeople;
  }

  Boolean init() override {
    return bankOfBoat == east && whoIsOnBank == Banks{allPeople, {}};
  }

  Boolean next() override {
    return (bankOfBoat == east && $E(passengers, candidatePassengers) {
      return passengers $in get_mem(whoIsOnBank, east) && move(passengers);
    }) || (bankOfBoat == west && $E(passengers, candidatePassengers) {
      return passengers $in get_mem(whoIsOnBank, west) && move(passengers);
    });
  }

  std::optional<Boolean> ensure() override {
    return typeOk();
  }

  std::optional<Boolean> stop() override {
    return get_mem(whoIsOnBank, east) == People{};
  }

  Var<int> bankOfBoat{"bank_of_boat"};
  Var<Banks> whoIsOnBank{"who_is_on_bank"};

  int east = 0;
  int west = 1;
  std::set<int> riverBanks = {east, west};
  People missionaries = {1, 2, 3};
  People cannibals = {11, 12, 13};
  People allPeople = {1, 2, 3, 11, 12, 13};
  PeopleChoices candidatePassengers = passengerChoices(allPeople);
};

TEST_F(EngineFixture, MissionariesAndCannibals) {
  e.createModel<Model>();
  ASSERT_NO_THROW(e.run());
}

}  // namespace missionaries_and_cannibals
