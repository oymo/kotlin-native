import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

fun main() {
    val a = AtomicReference<Any?>(null)
    val b = AtomicReference<Any?>(null)
    a.value = b
    b.value = a
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    val cycle = GC.findCycle(cycles[0])!!
    assertEquals(2, cycle.size)
    assertTrue(cycle.contains(a))
    assertTrue(cycle.contains(b))
    a.value = null
}
