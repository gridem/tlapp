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

PeopleChoices powerSet(const People& people) {
  PeopleChoices result = {People{}};
  for (auto&& person : people) {
    PeopleChoices next = result;
    for (auto&& subset : result) {
      auto extended = subset;
      extended.insert(person);
      next.insert(std::move(extended));
    }
    result = std::move(next);
  }
  return result;
}

int otherBank(int bank) { return bank == 0 ? 1 : 0; }

People peopleOnBank(const Banks& banks, int bank) {
  return bank == 0 ? banks.east : banks.west;
}

Banks moveAcross(int bank, const People& thisBank, const People& otherBankPeople) {
  if (bank == 0) {
    return Banks{thisBank, otherBankPeople};
  }
  return Banks{otherBankPeople, thisBank};
}

// See TLA+ spec details here:
// https://github.com/tlaplus/Examples/blob/master/specifications/MissionariesAndCannibals/MissionariesAndCannibals.tla
struct Model : IModel {
  Boolean isSafe(auto people) {
    return people $in cannibals ||
           get_mem((people $cap cannibals), size()) <=
               get_mem((people $cap missionaries), size());
  }

  Boolean move(auto passengers) {
    auto currentBank = evaluator_fun(peopleOnBank, whoIsOnBank, bankOfBoat);
    auto destinationBank = evaluator_fun(otherBank, bankOfBoat);
    auto destinationPeople =
        evaluator_fun(peopleOnBank, whoIsOnBank, destinationBank);
    auto remaining = currentBank $diff passengers;
    auto arrived = destinationPeople $cup passengers;
    auto passengerCount = get_mem(passengers, size());

    return passengerCount >= 1 && passengerCount <= 2 && isSafe(remaining) &&
           isSafe(arrived) && bankOfBoat++ == destinationBank &&
           whoIsOnBank++ ==
               evaluator_fun(moveAcross, bankOfBoat, remaining, arrived);
  }

  Boolean typeOk() {
    auto eastBank = get_mem(whoIsOnBank, east);
    auto westBank = get_mem(whoIsOnBank, west);
    return (bankOfBoat $in riverBanks) && isSafe(eastBank) && isSafe(westBank) &&
           (eastBank $cap westBank) == People{} &&
           (eastBank $cup westBank) == allPeople;
  }

  Boolean init() override {
    return bankOfBoat == east && whoIsOnBank == Banks{allPeople, {}};
  }

  Boolean next() override {
    return $E(passengers,
              evaluator_fun(powerSet,
                            evaluator_fun(peopleOnBank, whoIsOnBank,
                                          bankOfBoat))) {
      return move(passengers);
    };
  }

  std::optional<Boolean> ensure() override { return typeOk(); }

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
};

TEST_F(EngineFixture, MissionariesAndCannibals) {
  e.createModel<Model>();
  ASSERT_NO_THROW(e.run());
}

}  // namespace missionaries_and_cannibals
