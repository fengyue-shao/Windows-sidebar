#include "include/clipboardmanager.h"
#include <QCoreApplication>
#include <iostream>
#define CHECK(v) do { if (!(v)) { std::cerr << "FAIL " << __LINE__ << '\n'; return 1; } } while(false)
int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
    ClipboardManager model(nullptr,false); // No settings or system API calls.
    const QVariantMap pinned{{"id","p"},{"pinned",true}};
    const QVariantMap first{{"id","a"},{"pinned",false}};
    const QVariantMap failed{{"id","b"},{"pinned",false}};
    model.applyHistory({pinned,first,failed},"fixture");
    model.applyClearResult({"a"},"one deletion failed");
    CHECK(model.items()==QVariantList({pinned,failed}));
    CHECK(model.statusText()=="one deletion failed");
    model.applyClearResult({},"failed"); CHECK(model.items().size()==2);
    model.applyClearResult({"not-displayed"},"outside view"); CHECK(model.items().size()==2);
    model.applyDeleteResult("b",false,"denied"); CHECK(model.items().size()==2);
    model.applyDeleteResult("b",true,"confirmed"); CHECK(model.items()==QVariantList({pinned}));
    std::cout << "CLIPBOARD RESULT PASS: only confirmed IDs removed; partial/all failures and pinned row retained; no system clipboard/settings access\n";
}
