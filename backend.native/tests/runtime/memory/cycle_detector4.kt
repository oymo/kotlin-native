import kotlin.native.concurrent.*
import kotlin.test.*

class Holder(var other: Any?)

fun main() {
    val atomic = AtomicReference<Any?>(null)
    atomic.value = Pair(atomic, Holder(atomic)).freeze()
}
