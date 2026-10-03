#include "include/pluginprotocol.h"
#include <QCoreApplication>
#include <iostream>
#define CHECK(v) do { if (!(v)) { std::cerr << "FAIL " << __LINE__ << '\n'; return 1; } } while(false)
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    using namespace PluginProtocol;
    Decoder decoder;
    CHECK(decoder.feed("{\"type\":").isEmpty());
    const auto frames=decoder.feed("\"hello\"}\n{\"type\":\"state\"}\n");
    CHECK(frames.size()==2 && !decoder.failed);
    Decoder bad; CHECK(bad.feed("{broken}\n").isEmpty() && bad.failed);
    CHECK(bad.feed("{}\n").isEmpty());
    Decoder array; CHECK(array.feed("[]\n").isEmpty() && array.failed);
    Decoder oversized; CHECK(oversized.feed(QByteArray(MaxFrameBytes+1,'x')).isEmpty() && oversized.failed);
    const QByteArray token(64,'a');
    Session session("test.plugin",token,{"main"}); Message message;
    QJsonObject state{{"type","state"},{"source","running"},{"priority",10},
        {"state",QJsonObject{{"title","Task"},{"pageId","main"},{"progress",0.5}}}};
    CHECK(!session.accept(state,0,message));
    QJsonObject hello{{"type","hello"},{"protocolVersion",Version},{"pluginId","test.plugin"},{"token",QString::fromLatin1(token)}};
    auto wrong=hello; wrong["token"]="wrong"; CHECK(!session.accept(wrong,0,message));
    wrong=hello; wrong["protocolVersion"]=2; CHECK(!session.accept(wrong,0,message));
    wrong=hello; wrong["pluginId"]="other"; CHECK(!session.accept(wrong,0,message));
    CHECK(session.accept(hello,0,message) && session.authenticated());
    CHECK(!session.accept(hello,0,message));
    CHECK(session.accept(state,0,message));
    CHECK(message.sourceId==namespaceFor("test.plugin")+".running");
    CHECK(message.state.value("route")==namespaceFor("test.plugin")+".main");
    CHECK(message.state.value("kind")=="task");
    wrong=state; wrong["source"]="builtin.media"; CHECK(!session.accept(wrong,0,message));
    wrong=state; wrong["priority"]=61; CHECK(!session.accept(wrong,0,message));
    wrong=state; wrong["priority"]=0.5; CHECK(!session.accept(wrong,0,message));
    auto fields=state["state"].toObject(); fields["route"]="settings";
    wrong=state; wrong["state"]=fields; CHECK(!session.accept(wrong,0,message));
    fields.remove("route"); fields["pageId"]="settings"; wrong["state"]=fields; CHECK(!session.accept(wrong,0,message));
    fields["pageId"]="main"; fields["progress"]=1.1; wrong["state"]=fields; CHECK(!session.accept(wrong,0,message));
    fields["progress"]=0.5; fields["title"]=QString(129,'x'); wrong["state"]=fields; CHECK(!session.accept(wrong,0,message));
    for (int i=0;i<7;++i) { wrong=state; wrong["source"]=QString("s%1").arg(i); CHECK(session.accept(wrong,1000,message)); }
    wrong["source"]="ninth"; CHECK(!session.accept(wrong,1000,message));
    CHECK(session.accept({{"type","withdraw"},{"source","s0"}},1000,message));
    CHECK(session.accept(wrong,1000,message));
    auto event=state; event["type"]="event"; event["key"]="done"; event["priority"]=95;
    CHECK(session.accept(event,2000,message) && message.durationMs==4000);
    event["durationMs"]=10001; CHECK(!session.accept(event,2000,message));
    event["durationMs"]=4000; event["priority"]=96; CHECK(!session.accept(event,2000,message));
    int accepted=0; for (int i=0;i<40;++i) if (session.accept(state,3000,message)) ++accepted;
    CHECK(accepted==30); CHECK(session.accept(state,4000,message));
    CHECK(session.sourceIds().size()==8);
    std::cout << "PLUGIN PROTOCOL PASS: fragmented/batched bounded frames, strict handshake, source/route ownership, priorities, fields, events, source cap, rate limit; no process/system actions\n";
}
