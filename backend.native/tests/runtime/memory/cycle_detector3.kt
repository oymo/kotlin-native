import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

class Holder(var other: Any?)

fun main() {
    val a1 = FreezableAtomicReference<Any?>(null)
    val head = Holder(null)
    var current = head
    repeat(30) {
        val next = Holder(null)
        current.other = next
        current = next
    }
    a1.value = head
    current.other = a1
    current.freeze()
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    val cycle = GC.findCycle(cycles[0])!!
    assertEquals(32, cycle.size)
    a1.value = null
}
