import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

class Holder(var other: Any?)

fun main() {
    val array = arrayOf(AtomicReference<Any?>(null), AtomicReference<Any?>(null))
    val obj1 = Holder(array).freeze()
    array[0].value = obj1
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    assertTrue(arrayOf(obj1, array, array[0]).contentEquals(GC.findCycle(cycles[0])!!))
    array[0].value = null
}
