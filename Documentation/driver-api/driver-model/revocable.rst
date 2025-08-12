.. SPDX-License-Identifier: GPL-2.0

==============================
Revocable Resource Management
==============================

Overview
========

.. kernel-doc:: drivers/base/revocable.c
   :doc: Overview

Revocable vs. Device-Managed (devm) Resources
=============================================

It's important to understand the distinction between a standard
device-managed (devm) resource and a resource managed by a revocable provider.

The key difference is their lifetime:

*   A **devm resource** is tied to the lifetime of the device.  It is
    automatically freed when the device is unbound.
*   A **revocable provider** persists as long as there are active references
    to it from consumer handles.

This means that a revocable provider can outlive the device that created
it.  This is a deliberate design feature, allowing consumers to hold a
reference to a resource even after the underlying device has been removed,
without causing a fault.  When the consumer attempts to access the resource,
it will simply be informed that the resource is no longer available.

API and Usage
=============

For Resource Providers
----------------------

.. kernel-doc:: drivers/base/revocable.c
   :identifiers: revocable_provider_alloc

.. kernel-doc:: drivers/base/revocable.c
   :identifiers: devm_revocable_provider_alloc

.. kernel-doc:: drivers/base/revocable.c
   :identifiers: revocable_provider_revoke

For Resource Consumers
----------------------

.. kernel-doc:: drivers/base/revocable.c
   :identifiers: revocable_alloc

.. kernel-doc:: drivers/base/revocable.c
   :identifiers: revocable_free

.. kernel-doc:: drivers/base/revocable.c
   :identifiers: revocable_try_access

.. kernel-doc:: drivers/base/revocable.c
   :identifiers: revocable_withdraw_access

.. kernel-doc:: include/linux/revocable.h
   :identifiers: REVOCABLE_TRY_ACCESS_WITH

Example Usage
~~~~~~~~~~~~~

.. code-block:: c

    void consumer_use_resource(struct revocable *rev)
    {
        struct foo_resource *res;

        REVOCABLE_TRY_ACCESS_WITH(rev, res) {
            // Always check if the resource is valid.
            if (!res) {
                pr_warn("Resource is not available\n");
                return;
            }

            // At this point, 'res' is guaranteed to be valid until
            // this block exits.
            do_something_with(res);
        }

        // revocable_withdraw_access() is automatically called here.
    }
