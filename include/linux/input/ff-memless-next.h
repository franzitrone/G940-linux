#include <linux/input.h>

/** DEFINITION OF TERMS
 *
 * Combined effect - An effect whose force is a superposition of forces
 *                   generated by all effects that can be added together.
 *                   Only one combined effect can be playing at a time.
 *                   Effects that can be added together to create a combined
 *                   effect are FF_CONSTANT, FF_PERIODIC and FF_RAMP.
 * Uncombinable effect - An effect that cannot be combined with another effect.
 *                       All conditional effects - FF_DAMPER, FF_FRICTION,
 *                       FF_INERTIA and FF_SPRING are uncombinable.
 *                       Number of uncombinable effects playing simultaneously
 *                       depends on the capabilities of the hardware.
 * Rumble effect - An effect generated by device's rumble motors instead of
 *                 force feedback actuators.
 *
 *
 * HANDLING OF UNCOMBINABLE EFFECTS
 *
 * Uncombinable effects cannot be combined together into just one effect, at
 * least not in a clear and obvious manner. Therefore these effects have to
 * be handled individually by ff-memless-next. Handling of these effects is
 * left entirely to the hardware-specific driver, ff-memless-next merely
 * passes these effects to the hardware-specific driver at appropriate time.
 * ff-memless-next provides the UPLOAD command to notify the hardware-specific
 * driver that the userspace is about to request playback of an uncombinable
 * effect. The hardware-specific driver shall take all steps needed to make
 * the device ready to play the effect when it receives the UPLOAD command.
 * The actual playback shall commence when START_UNCOMB command is received.
 * Opposite to the UPLOAD command is the ERASE command which tells
 * the hardware-specific driver that the playback has finished and that
 * the effect will not be restarted. STOP_UNCOMB command tells
 * the hardware-specific driver that the playback shall stop but the device
 * shall still be ready to resume the playback immediately.
 *
 * In case it is not possible to make the device ready to play an uncombinable
 * effect (all hardware effect slots are occupied), the hardware-specific
 * driver may return an error when it receives an UPLOAD command. If the
 * hardware-specific driver returns 0, the upload is considered successful.
 * START_UNCOMB and STOP_UNCOMB commands cannot fail and the device must always
 * start the playback of the requested effect if the UPLOAD command of the
 * respective effect has been successful. ff-memless-next will never send
 * a START/STOP_UNCOMB command for an effect that has not been uploaded
 * successfully, nor will it send an ERASE command for an effect that is
 * playing (= has been started with START_UNCOMB command).
 */

enum mlnx_commands {
	/* Start or update a combined effect. This command is sent whenever
	 * a FF_CONSTANT, FF_PERIODIC or FF_RAMP is started, stopped or
	 * updated by userspace, when the applied envelopes are recalculated
	 * or when periodic effects are recalculated. */
	MLNX_START_COMBINED,
	/* Stop combined effect. This command is sent when all combinable
	 * effects are stopped. */
	MLNX_STOP_COMBINED,
	/* Start or update a rumble effect. This command is sent whenever
	 * a FF_RUMBLE effect is started or when its magnitudes or directions
	 * change. */
	MLNX_START_RUMBLE,
	/* Stop a rumble effect. This command is sent when all FF_RUMBLE
	 * effects are stopped. */
	MLNX_STOP_RUMBLE,
	/* Start or update an uncombinable effect. This command is sent
	 * whenever an uncombinable effect is started or updated. */
	MLNX_START_UNCOMB,
	/* Stop uncombinable effect. This command is sent when an uncombinable
	 * effect is stopped. */
	MLNX_STOP_UNCOMB,
	/* Upload uncombinable effect to device. This command is sent when the
	 * effect is started from userspace. It is up to the hardware-specific
	 * driver to handle this situation.
	 */
	MLNX_UPLOAD_UNCOMB,
	/* Remove uncombinable effect from device, This command is sent when
	 * and uncombinable effect has finished playing and will not be
	 * restarted.
	 */
	MLNX_ERASE_UNCOMB
};

/** struct mlnx_simple_force - holds constant forces along X and Y axis
 * @x: Force along X axis. Negative value denotes force pulling to the left,
 *     positive value denotes force pulling to the right.
 * @y: Force along Y axis. Negative value denotes force denotes force pulling
 *     away from the user, positive value denotes force pulling towards
 *     the user.
 */
struct mlnx_simple_force {
	const s32 x;
	const s32 y;
};

/** struct mlnx_rumble_force - holds information about rumble effect
 * @strong: Magnitude of the strong vibration.
 * @weak: Magnitude of the weak vibration.
 * @strong_dir: Direction of the strong vibration expressed in the same way
 *              as the direction of force feedback effect in struct ff_effect.
 * @weak_dir: Direction of the weak vibration, same as above applies.
 */
struct mlnx_rumble_force {
	const u32 strong;
	const u32 weak;
	const u16 strong_dir;
	const u16 weak_dir;
};

/** struct mlnx_uncomb_effect - holds information about uncombinable effect
 * @id: Id of the effect assigned by ff-core.
 * @effect: Pointer to the uncombinable effect stored in ff-memless-next module
 *          Hardware-specific driver must not alter this.
 */
struct mlnx_uncomb_effect {
	const int id;
	const struct ff_effect *effect;
};

/** struct mlnx_commands - describes what action shall the force feedback
 *                         device perform
 * @cmd: Type of the action.
 * @u: Data associated with the action.
 */
struct mlnx_effect_command {
	const enum mlnx_commands cmd;
	union {
		const struct mlnx_simple_force simple_force;
		const struct mlnx_rumble_force rumble_force;
		const struct mlnx_uncomb_effect uncomb;
	} u;
};

/** input_ff_create_mlnx() - Register a device within ff-memless-next and
 *                           the kernel force feedback system
 * @dev: Pointer to the struct input_dev associated with the device.
 * @data: Any device-specific data that shall be passed to the callback.
 *        function called by ff-memless-next when a force feedback action
 *        shall be performed.
 * @control_effect: Pointer to the callback function.
 * @update_date: Delay in milliseconds between two recalculations of periodic
 *               effects, ramp effects and envelopes. Note that this value will
 *               never be lower than (CONFIG_HZ / 1000) + 1 regardless of the
 *               value specified here. This is not a "hard" rate limiter.
 *               Userspace still can submit effects at a rate faster than
 *               this value.
 */
int input_ff_create_mlnx(struct input_dev *dev, void *data,
			 int (*control_effect)(struct input_dev *, void *, const struct mlnx_effect_command *),
			 const u16 update_rate);

/** int control_effect() - Callback to the HW-specific driver.
 * @struct input_dev *: Pointer to the struct input_dev of the device that is
 *                     is being controlled.
 * @void *: Pointer to any device-specific data set by the HW-specific driver.
 *         This data will be free'd automatically by ff-memless-next when the
 *         device is destroyed.
 * @const struct mlnx_effect_command *:
 *         Action the device shall perform. Note that this pointer is valid
 *         only within the context of the callback function. If the HW-specific
 *         driver needs any data from this structure after the callback
 *         function returns, it must copy it.
 */