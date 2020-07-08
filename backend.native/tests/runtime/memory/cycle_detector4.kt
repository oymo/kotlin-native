import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

class Holder(var other: Any?)

fun main() {
    val atomic1 = AtomicReference<Any?>(null)
    val atomic2 = AtomicReference<Any?>(null)
    atomic1.value = atomic2
    val cycles = GC.detectCycles()!!
    assertEquals(0, cycles.size)
}
