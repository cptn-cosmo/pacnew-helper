// SPDX-License-Identifier: GPL-3.0-or-later

#include "helper.h"
#include "operations.h"

#include <KAuth/HelperSupport>

using namespace Qt::StringLiterals;

namespace
{
KAuth::ActionReply failure(const QString &message)
{
    KAuth::ActionReply reply = KAuth::ActionReply::HelperErrorReply();
    reply.setErrorDescription(message);
    return reply;
}
} // namespace

KAuth::ActionReply PacnewAuthHelper::apply(const QVariantMap &args)
{
    const auto op = Ops::parseOperation(args.value(u"operation"_s).toString());
    if (!op)
        return failure(u"Unknown operation"_s);
    const Ops::Result result = Ops::apply(Ops::Context{},
                                          args.value(u"path"_s).toString(),
                                          args.value(u"kind"_s).toString(),
                                          *op,
                                          args.value(u"content"_s).toByteArray());
    if (!result.ok)
        return failure(result.error);
    KAuth::ActionReply reply = KAuth::ActionReply::SuccessReply();
    reply.addData(u"backupId"_s, result.backupId);
    return reply;
}

KAuth::ActionReply PacnewAuthHelper::read(const QVariantMap &args)
{
    const Ops::ReadResult result = Ops::read(Ops::Context{}, args.value(u"path"_s).toString(), args.value(u"kind"_s).toString());
    if (!result.ok)
        return failure(result.error);
    KAuth::ActionReply reply = KAuth::ActionReply::SuccessReply();
    reply.addData(u"live"_s, result.live);
    reply.addData(u"sidecar"_s, result.sidecar);
    reply.addData(u"liveExists"_s, result.liveExists);
    return reply;
}

KAuth::ActionReply PacnewAuthHelper::restore(const QVariantMap &args)
{
    const Ops::Result result = Ops::restore(Ops::Context{}, args.value(u"backupId"_s).toString());
    if (!result.ok)
        return failure(result.error);
    KAuth::ActionReply reply = KAuth::ActionReply::SuccessReply();
    reply.addData(u"backupId"_s, result.backupId);
    return reply;
}

KAUTH_HELPER_MAIN("gay.geeks.pacnewhelper", PacnewAuthHelper)

#include "moc_helper.cpp"
