#define CATCH_CONFIG_RUNNER // use custom main function
#include "../3rd-party/catch2/catch.hpp"

struct EventListener : Catch::TestEventListenerBase {
    using TestEventListenerBase::TestEventListenerBase;

    std::vector<std::string> failedCases;

    void testCaseStarting(Catch::TestCaseInfo const & testInfo) override {
        Catch::TestEventListenerBase::testCaseStarting(testInfo);
        printf("\n\n"
               "===============================================================================\n"
               "Starting %s\n"
               "-------------------------------------------------------------------------------\n",
               testInfo.name.c_str());
    }

    void testCaseEnded(Catch::TestCaseStats const & testCaseStats) override {
        Catch::TestEventListenerBase::testCaseEnded(testCaseStats);
        if (testCaseStats.totals.assertions.failed > 0) {
            // store failed test
            failedCases.push_back(testCaseStats.testInfo.name);
        }
    }

    void testRunEnded(Catch::TestRunStats const & stats) override {
        Catch::TestEventListenerBase::testRunEnded(stats);
        if (!failedCases.empty()) {
            printf("Failed cases:\n");
            for (const auto & c : failedCases) printf("    %s\n", c.c_str());
        }
    }
};

CATCH_REGISTER_LISTENER(EventListener)

int main(int argc, char * argv[]) {
    Catch::Session session;
    auto           ret = session.applyCommandLine(argc, argv);
    if (ret) { return ret; }
    return session.run();
}
