import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

class Holder {
    lateinit var other: WorkerBoundReference<Any>
}

fun main() {
    val holder = Holder()
    val a: AtomicReference<WorkerBoundReference<Holder>?> = AtomicReference(WorkerBoundReference(holder))
    val b = WorkerBoundReference(a)
    holder.other = b
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    val cycle = GC.findCycle(cycles[0])!!
    assertEquals(4, cycle.size)
    assertTrue(cycle.contains(holder))
    assertTrue(cycle.contains(a))
    assertTrue(cycle.contains(a.value!!))
    assertTrue(cycle.contains(b))
    a.value = null
}
