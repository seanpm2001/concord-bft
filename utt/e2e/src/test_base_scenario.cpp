#include <xutils/Log.h>

using namespace libutt;

struct E2eTestContext {
  std::unique_ptr<Wallet> wallet1, wallet2, wallet3;
  Admin::Channel chanAdmin;
  Wallet::Channel chanWallet;
};

class E2eTestScenario {
 protected:
  E2eTestContext &context;

 public:
  E2eTestScenario(E2eTestContext &context) : context(context) {}

  virtual int execute() = 0;

  virtual ~E2eTestScenario() {}
};

class E2eTestBaseScenario : public E2eTestScenario {
 public:
  E2eTestBaseScenario(E2eTestContext &context) : E2eTestScenario(context) {}
  int execute() override {
    context.wallet1->mint(context.chanWallet, 2000);
    Admin::createPrivacyBudget(context.chanAdmin, "user-1", 10000);
    sleep(5);
    uint64_t publicBalance, privateBalance;
    const uint64_t EXPECTED_PUBLIC_BALANCE_1_BEFORE = 8000;
    const uint64_t EXPECTED_PRIVATE_BALANCE_1_BEFORE = 2000;
    const uint64_t EXPECTED_PUBLIC_BALANCE_2_BEFORE = 10000;
    const uint64_t EXPECTED_PRIVATE_BALANCE_2_BEFORE = 0;

    std::tie(publicBalance, privateBalance) = context.wallet1->getPublicAndPrivateBalance(context.chanWallet);
    logdbg << "publicBalance1 before: " << publicBalance << ", privateBalance1 before: " << privateBalance << std::endl;
    if (publicBalance != EXPECTED_PUBLIC_BALANCE_1_BEFORE or privateBalance != EXPECTED_PRIVATE_BALANCE_1_BEFORE)
      return 1;

    std::tie(publicBalance, privateBalance) = context.wallet2->getPublicAndPrivateBalance(context.chanWallet);
    logdbg << "publicBalance2 before: " << publicBalance << ", privateBalance2 before: " << privateBalance << std::endl;
    if (publicBalance != EXPECTED_PUBLIC_BALANCE_2_BEFORE or privateBalance != EXPECTED_PRIVATE_BALANCE_2_BEFORE)
      return 1;

    context.wallet1->transfer(context.chanWallet, 800, "user-2");
    context.wallet1->burn(context.chanWallet, 700);

    const uint64_t EXPECTED_PUBLIC_BALANCE_1_AFTER = 8700;
    const uint64_t EXPECTED_PRIVATE_BALANCE_1_AFTER = 500;
    const uint64_t EXPECTED_PUBLIC_BALANCE_2_AFTER = 10000;
    const uint64_t EXPECTED_PRIVATE_BALANCE_2_AFTER = 800;

    std::tie(publicBalance, privateBalance) = context.wallet1->getPublicAndPrivateBalance(context.chanWallet);
    logdbg << "publicBalance1 after: " << publicBalance << ", privateBalance1 after: " << privateBalance << std::endl;
    if (publicBalance != EXPECTED_PUBLIC_BALANCE_1_AFTER or privateBalance != EXPECTED_PRIVATE_BALANCE_1_AFTER)
      return 1;

    std::tie(publicBalance, privateBalance) = context.wallet2->getPublicAndPrivateBalance(context.chanWallet);
    logdbg << "publicBalance2 after: " << publicBalance << ", privateBalance2 after: " << privateBalance << std::endl;
    if (publicBalance != EXPECTED_PUBLIC_BALANCE_2_AFTER or privateBalance != EXPECTED_PRIVATE_BALANCE_2_AFTER)
      return 1;

    logdbg << "TEST PASSED\n";
    return 0;
  }
};