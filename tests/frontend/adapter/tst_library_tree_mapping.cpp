#include <QList>
#include <QObject>
#include <QString>
#include <QtTest/QTest>

namespace {
struct FakeLibraryNode {
    QString id;
    QString label;
    QList<FakeLibraryNode> children;
};

struct QtFacingLibraryNode {
    QString key;
    QString title;
    int childCount;
    bool expandable;
};

QtFacingLibraryNode mapFakeLibraryNode(const FakeLibraryNode &node)
{
    return {
        node.id,
        node.label,
        static_cast<int>(node.children.size()),
        !node.children.isEmpty(),
    };
}
}

class LibraryTreeMappingTest : public QObject {
    Q_OBJECT

private slots:
    void mapsLibraryTreeNodeMetadata();
};

void LibraryTreeMappingTest::mapsLibraryTreeNodeMetadata()
{
    const FakeLibraryNode node{
        QStringLiteral("library/root"),
        QStringLiteral("Library"),
        {
            {QStringLiteral("library/root/albums"), QStringLiteral("Albums"), {}},
            {QStringLiteral("library/root/artists"), QStringLiteral("Artists"), {}},
        },
    };

    const QtFacingLibraryNode mapped = mapFakeLibraryNode(node);

    QCOMPARE(mapped.key, QStringLiteral("library/root"));
    QCOMPARE(mapped.title, QStringLiteral("Library"));
    QCOMPARE(mapped.childCount, 2);
    QCOMPARE(mapped.expandable, true);
}

QTEST_GUILESS_MAIN(LibraryTreeMappingTest)

#include "tst_library_tree_mapping.moc"
