package ch.sideffects.findmyanything

import android.util.Log
import io.rebble.pebblekit2.client.BasePebbleListenerService
import io.rebble.pebblekit2.common.model.PebbleDictionary
import io.rebble.pebblekit2.common.model.PebbleDictionaryItem
import io.rebble.pebblekit2.common.model.ReceiveResult
import io.rebble.pebblekit2.common.model.WatchIdentifier
import java.util.UUID

/** Bound by the Pebble app while the watchapp runs; forwards its commands to the engine. */
class PebbleListenerService : BasePebbleListenerService() {
    override fun onCreate() {
        super.onCreate()
        FinderEngine.init(this)
    }

    override suspend fun onMessageReceived(
        watchappUUID: UUID,
        data: PebbleDictionary,
        watch: WatchIdentifier,
    ): ReceiveResult {
        if (watchappUUID != Protocol.WATCHAPP_UUID) return ReceiveResult.Nack
        val cmd = data[Protocol.KEY_CMD]?.asInt() ?: Protocol.CMD_LIST
        val target = data[Protocol.KEY_TARGET]?.asInt()
        Log.d("FindMyAnything", "watch cmd=$cmd target=$target")
        FinderEngine.watchCommand(cmd, target)
        return ReceiveResult.Ack
    }

    override fun onAppOpened(watchappUUID: UUID, watch: WatchIdentifier) {
        if (watchappUUID == Protocol.WATCHAPP_UUID) FinderEngine.watchOpened()
    }

    override fun onAppClosed(watchappUUID: UUID, watch: WatchIdentifier) {
        if (watchappUUID == Protocol.WATCHAPP_UUID) FinderEngine.watchClosed()
    }
}

fun PebbleDictionaryItem.asInt(): Int? = when (this) {
    is PebbleDictionaryItem.Int8 -> value.toInt()
    is PebbleDictionaryItem.UInt8 -> value.toInt()
    is PebbleDictionaryItem.Int16 -> value.toInt()
    is PebbleDictionaryItem.UInt16 -> value.toInt()
    is PebbleDictionaryItem.Int32 -> value
    is PebbleDictionaryItem.UInt32 -> value.toInt()
    else -> null
}
