#include <QObject>
#include <QString>
#include <QtTest/QTest>

namespace {
enum class FakeCommandState {
    Accepted,
    Unsupported,
};

struct FakeBackendCommandResult {
    FakeCommandState state;
    QString detail;
};

struct QtFacingCommandResult {
    bool accepted;
    QString errorText;
};

QtFacingCommandResult mapFakeCommandResult(const FakeBackendCommandResult &result)
{
    switch (result.state) {
    case FakeCommandState::Accepted:
        return {true, QString()};
    case FakeCommandState::Unsupported:
        return {false, result.detail};
    }

    return {false, QString()};
}
}

class CommandResultMappingTest : public QObject {
    Q_OBJECT

private slots:
    void rejectsUnsupportedCommandResult();
};

void CommandResultMappingTest::rejectsUnsupportedCommandResult()
{
    const FakeBackendCommandResult result{FakeCommandState::Unsupported, QStringLiteral("unsupported command")};

    const QtFacingCommandResult mapped = mapFakeCommandResult(result);

    QCOMPARE(mapped.accepted, false);
    QCOMPARE(mapped.errorText, QStringLiteral("unsupported command"));
}

QTEST_GUILESS_MAIN(CommandResultMappingTest)

#include "tst_command_result_mapping.moc"
