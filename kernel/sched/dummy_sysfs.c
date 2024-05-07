#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>

static struct kset *msm_perf_kset;
static struct kobject *param_kobj;

// Dummy variables to store the min and max frequency values
static unsigned int dummy_min_freq = 300000;
static unsigned int dummy_max_freq = 2920000;

static ssize_t get_cpu_min_freq(struct kobject *kobj,
  struct kobj_attribute *attr, char *buf)
{
  int bytes_written = scnprintf(buf, PAGE_SIZE, "%u", dummy_min_freq);
  buf[bytes_written] = '0'; // Ensure null-termination
  return bytes_written;
}

static ssize_t set_cpu_min_freq(struct kobject *kobj,
  struct kobj_attribute *attr, const char *buf,
  size_t count)
{
  sscanf(buf, "%u", &dummy_min_freq);
  return count;
}

static ssize_t get_cpu_max_freq(struct kobject *kobj,
  struct kobj_attribute *attr, char *buf)
{
  int bytes_written = scnprintf(buf, PAGE_SIZE, "%u", dummy_max_freq);
  buf[bytes_written] = '0'; // Ensure null-termination
  return bytes_written;
}

static ssize_t set_cpu_max_freq(struct kobject *kobj,
  struct kobj_attribute *attr, const char *buf,
  size_t count)
{
  sscanf(buf, "%u", &dummy_max_freq);
  return count;
}

static struct kobj_attribute cpu_min_freq_attr =
  __ATTR(cpu_min_freq, 0644, get_cpu_min_freq, set_cpu_min_freq);
static struct kobj_attribute cpu_max_freq_attr =
  __ATTR(cpu_max_freq, 0644, get_cpu_max_freq, set_cpu_max_freq);

static struct attribute *param_attrs[] = {
  &cpu_min_freq_attr.attr,
  &cpu_max_freq_attr.attr,
  NULL,
};

static struct attribute_group param_attr_group = {
  .attrs = param_attrs,
};

static struct kobject *events_kobj;
static bool dummy_cpu_hotplug;

// Handlers for the cpu_hotplug sysfs node
static ssize_t get_cpu_hotplug(struct kobject *kobj,
  struct kobj_attribute *attr, char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%u", dummy_cpu_hotplug);
}


static struct kobj_attribute cpu_hotplug_attr =
  __ATTR(cpu_hotplug, 0444, get_cpu_hotplug, NULL);


static struct attribute *events_attrs[] = {
  &cpu_hotplug_attr.attr,
  NULL,
};

static struct attribute_group events_attr_group = {
  .attrs = events_attrs,
};

static int add_module_params(void)
{
  int ret;

  param_kobj = kobject_create_and_add("parameters", &msm_perf_kset->kobj);
  if (!param_kobj) {
    pr_err("msm_perf: Failed to add param_kobj\n");
    return -ENOMEM;
  }

  ret = sysfs_create_group(param_kobj, &param_attr_group);
  if (ret) {
    pr_err("msm_perf: Failed to create sysfs group\n");
    kobject_put(param_kobj);
    return ret;
  }

    // Create events group
  events_kobj = kobject_create_and_add("events", &msm_perf_kset->kobj);
  if (!events_kobj) {
    pr_err("msm_perf: Failed to add events_kobj\n");
    kobject_put(events_kobj); // Roll back the parameters group creation
    return -ENOMEM;
  }

  ret = sysfs_create_group(events_kobj, &events_attr_group);
    if (ret) {
      pr_err("msm_perf: Failed to create events sysfs group\n");
      kobject_put(events_kobj);
      kobject_put(param_kobj); // Roll back the parameters group creation
      return ret;
    }


  return 0;
}

static int msm_performance_init(void)
{
  msm_perf_kset = kset_create_and_add("msm_performance", NULL, kernel_kobj);
  if (!msm_perf_kset)
    return -ENOMEM;

  return add_module_params();
}

static struct kobject *cpu0_core_ctl_kobj;
static struct kobject *cpu4_core_ctl_kobj;
static struct kobject *cpu7_core_ctl_kobj;

static unsigned int cpu0_min_cpus = 0;
static unsigned int cpu4_min_cpus = 0;
static unsigned int cpu7_min_cpus = 0;


static ssize_t get_cpu0_min_cpus(struct kobject *kobj,
  struct kobj_attribute *attr, char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%u\n", cpu0_min_cpus);
}

static ssize_t set_cpu0_min_cpus(struct kobject *kobj,
  struct kobj_attribute *attr, const char *buf,
  size_t count)
{
  int ret = kstrtouint(buf, 10, &cpu0_min_cpus);
  if (ret < 0)
    return ret;
  return count;
}


static ssize_t get_cpu4_min_cpus(struct kobject *kobj,
  struct kobj_attribute *attr, char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%u\n", cpu4_min_cpus);
}

static ssize_t set_cpu4_min_cpus(struct kobject *kobj,
  struct kobj_attribute *attr, const char *buf,
  size_t count)
{
  int ret = kstrtouint(buf, 10, &cpu4_min_cpus);
  if (ret < 0)
    return ret;
  return count;
}

static ssize_t get_cpu7_min_cpus(struct kobject *kobj,
  struct kobj_attribute *attr, char *buf)
{
  return scnprintf(buf, PAGE_SIZE, "%u\n", cpu7_min_cpus);
}

static ssize_t set_cpu7_min_cpus(struct kobject *kobj,
  struct kobj_attribute *attr, const char *buf,
  size_t count)
{
  int ret = kstrtouint(buf, 10, &cpu7_min_cpus);
  if (ret < 0)
    return ret;
  return count;
}

static struct kobj_attribute cpu0_min_cpus_attr =
  __ATTR(min_cpus, 0644, get_cpu0_min_cpus, set_cpu0_min_cpus);
static struct kobj_attribute cpu4_min_cpus_attr =
  __ATTR(min_cpus, 0644, get_cpu4_min_cpus, set_cpu4_min_cpus);
static struct kobj_attribute cpu7_min_cpus_attr =
  __ATTR(min_cpus, 0644, get_cpu7_min_cpus, set_cpu7_min_cpus);

static int create_core_ctl_entries(void)
{
  int ret;

  cpu0_core_ctl_kobj = kobject_create_and_add("core_ctl",
    &get_cpu_device(0)->kobj);
  if (!cpu0_core_ctl_kobj)
    return -ENOMEM;

  ret = sysfs_create_file(cpu0_core_ctl_kobj, &cpu0_min_cpus_attr.attr);
  if (ret){
    printk("CORE_CTL 0 init error");
    goto error;
    }

  cpu4_core_ctl_kobj = kobject_create_and_add("core_ctl",
    &get_cpu_device(4)->kobj);
  if (!cpu4_core_ctl_kobj)
    return -ENOMEM;

  ret = sysfs_create_file(cpu4_core_ctl_kobj, &cpu4_min_cpus_attr.attr);
  if (ret){
    printk("CORE_CTL 4 init error");
    goto error;
    }

  cpu7_core_ctl_kobj = kobject_create_and_add("core_ctl",
    &get_cpu_device(7)->kobj);
  if (!cpu7_core_ctl_kobj) {
    ret = -ENOMEM;
    goto error;
  }

  ret = sysfs_create_file(cpu7_core_ctl_kobj, &cpu7_min_cpus_attr.attr);
  if (ret)
    goto error;

  return 0;

error:
  if (cpu0_core_ctl_kobj)
          kobject_put(cpu0_core_ctl_kobj);
  if (cpu4_core_ctl_kobj)
    kobject_put(cpu4_core_ctl_kobj);
  if (cpu7_core_ctl_kobj)
    kobject_put(cpu7_core_ctl_kobj);
  return ret;
}

static int __init corectl_sysfs_init(void)
{
  return create_core_ctl_entries();
}



#define CPU_NUM 4 // Target CPU number

static ssize_t up_rate_limit_us_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    // This is a dummy read function that always returns 0.
    return sprintf(buf, "0\n");
}

static ssize_t up_rate_limit_us_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    // This is a dummy write function that does nothing.
    return count;
}

static struct kobject *walt_kobj;
static struct kobj_attribute up_rate_limit_us_attr = __ATTR(up_rate_limit_us, 0644, up_rate_limit_us_show, up_rate_limit_us_store);

static struct attribute *attrs[] = {
    &up_rate_limit_us_attr.attr,
    NULL,   // Need to terminate the list of attributes
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static struct kobject *cpufreq_kobj;


int cpufreq_dummy_init(void)
{
    int retval;
    struct kobject *cpufreq_kobj;
    struct cpufreq_policy *policy;

      policy = cpufreq_cpu_get(CPU_NUM);
      if (!policy) {
          printk(KERN_INFO "Failed to get cpufreq policy for CPU %d\n", CPU_NUM);
          return -ENOENT;
      }

    cpufreq_kobj = get_governor_parent_kobj(policy);
    // Create a new kobject for the walt directory within the cpufreq directory
    walt_kobj = kobject_create_and_add("walt", cpufreq_kobj);
    if (!walt_kobj) {
        printk(KERN_INFO "Failed to create walt kobject for CPU %d\n", CPU_NUM);
        cpufreq_cpu_put(policy);
        return -ENOMEM;
    }

    // Create the up_rate_limit_us file in the walt directory
    retval = sysfs_create_file(walt_kobj, &up_rate_limit_us_attr.attr);
    if (retval) {
        printk(KERN_INFO "Failed to create up_rate_limit_us sysfs entry\n");
        kobject_put(walt_kobj); // Clean up if we failed
        return retval;
    }

    printk(KERN_INFO "Dummy sysfs node /sys/devices/system/cpu/cpu%d/cpufreq/walt/up_rate_limit_us created\n", CPU_NUM);
    cpufreq_cpu_put(policy);
    return 0;
}
EXPORT_SYMBOL_GPL(cpufreq_dummy_init);

late_initcall(msm_performance_init);
late_initcall(corectl_sysfs_init);

